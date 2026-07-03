#include "nb_display_hal_shell.h"

#include "driver/spi_master.h"
#include "esp_attr.h"
#include "esp_cache.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_st7789.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define NB_DISPLAY_HAL_SPI_HOST SPI2_HOST
#define NB_DISPLAY_HAL_PIN_SCLK 12
#define NB_DISPLAY_HAL_PIN_MOSI 11
#define NB_DISPLAY_HAL_PIN_CS 10
#define NB_DISPLAY_HAL_PIN_DC 14
/* Causa real do "artefato de cor" não era clock nem fiação: o
 * esp_lcd_panel_io_spi (ESP-IDF) nunca seta a flag SPI_TRANS_DMA_USE_PSRAM
 * nas transações que monta, então o driver spi_master não sincroniza cache
 * antes do DMA ler um buffer alocado em PSRAM (confirmado lendo
 * esp_driver_spi/src/gpspi/spi_master.c: use_psram fica false, então o
 * cache_msync condicional na linha ~1216 nunca roda pra esse caso). Testado
 * e confirmado: buffer único em SRAM interna (sem PSRAM) ficou perfeito
 * até com padrão estático e 10 MHz; buffer em PSRAM sem sync manual
 * corrompia mesmo estático. Corrigido chamando esp_cache_msync() na cara
 * antes de cada draw_bitmap (ver flush_and_swap). 40 MHz confirmado limpo
 * com o sync manual — o clock nunca foi o problema. */
#define NB_DISPLAY_HAL_PIXEL_CLOCK_HZ (40 * 1000 * 1000)

static const char *TAG = "display_hal";
static nb_display_hal_t s_hal;
static esp_lcd_panel_handle_t s_panel;
static size_t s_buffer_bytes;

/* esp_lcd_panel_draw_bitmap() no SPI é assíncrono: enfileira a transferência
 * DMA e retorna antes de os pixels saírem pela SPI. Um frame cheio
 * (320x240x16bpp @ 20 MHz) leva ~61 ms para transmitir. Sem barreira, a task
 * de render (loop ~33 ms) sobrescreve o framebuffer enquanto o DMA ainda o lê
 * -> mistura de dois frames nas bandas ("flicker" que aparece e some no
 * batimento das duas cadências). Este semáforo serializa: só uma transferência
 * em voo, e o swap/reuso do buffer espera o `on_color_trans_done`. */
static SemaphoreHandle_t s_trans_done;

static bool IRAM_ATTR nb_display_hal_on_trans_done(esp_lcd_panel_io_handle_t io,
                                                   esp_lcd_panel_io_event_data_t *edata,
                                                   void *user_ctx)
{
    BaseType_t higher_prio_woken = pdFALSE;

    (void)io;
    (void)edata;
    (void)user_ctx;

    xSemaphoreGiveFromISR(s_trans_done, &higher_prio_woken);
    return higher_prio_woken == pdTRUE;
}

esp_err_t nb_display_hal_shell_init(void)
{
    const size_t buffer_pixels = (size_t)NB_DISPLAY_HAL_WIDTH * NB_DISPLAY_HAL_HEIGHT;
    const size_t buffer_bytes = buffer_pixels * sizeof(uint16_t);
    uint16_t *buffer_a;
    uint16_t *buffer_b;
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_err_t err;

    s_buffer_bytes = buffer_bytes;

    /* esp_cache_msync() exige endereço alinhado à linha de cache (32 bytes
     * no ESP32-S3) -- heap_caps_malloc não garante isso, só o tamanho já
     * é múltiplo de 32 por acaso (320x240x2). Usar aligned_alloc evita o
     * ESP_ERR_INVALID_ARG visto em bancada quando o 2º buffer caía
     * desalinhado. */
    buffer_a = heap_caps_aligned_alloc(32, buffer_bytes, MALLOC_CAP_SPIRAM);
    buffer_b = heap_caps_aligned_alloc(32, buffer_bytes, MALLOC_CAP_SPIRAM);
    if (buffer_a == NULL || buffer_b == NULL) {
        ESP_LOGE(TAG, "falha ao alocar framebuffers em PSRAM (%u bytes cada)",
                 (unsigned)buffer_bytes);
        return ESP_ERR_NO_MEM;
    }
    nb_display_hal_init(&s_hal, buffer_a, buffer_b);

    spi_bus_config_t bus_cfg = {
        .sclk_io_num = NB_DISPLAY_HAL_PIN_SCLK,
        .mosi_io_num = NB_DISPLAY_HAL_PIN_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = (int)buffer_bytes,
    };
    err = spi_bus_initialize(NB_DISPLAY_HAL_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_initialize falhou: %s", esp_err_to_name(err));
        return err;
    }

    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num = NB_DISPLAY_HAL_PIN_DC,
        .cs_gpio_num = NB_DISPLAY_HAL_PIN_CS,
        .pclk_hz = NB_DISPLAY_HAL_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    err = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)NB_DISPLAY_HAL_SPI_HOST, &io_cfg,
                                   &io_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_lcd_new_panel_io_spi falhou: %s", esp_err_to_name(err));
        return err;
    }

    s_trans_done = xSemaphoreCreateBinary();
    if (s_trans_done == NULL) {
        ESP_LOGE(TAG, "falha ao criar semáforo de transferência DMA");
        return ESP_ERR_NO_MEM;
    }
    /* Começa "livre": o primeiro flush não tem transferência anterior para
     * esperar. */
    xSemaphoreGive(s_trans_done);

    const esp_lcd_panel_io_callbacks_t io_cbs = {
        .on_color_trans_done = nb_display_hal_on_trans_done,
    };
    err = esp_lcd_panel_io_register_event_callbacks(io_handle, &io_cbs, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "register_event_callbacks falhou: %s", esp_err_to_name(err));
        return err;
    }

    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = -1, /* sem pino de RST dedicado -> SWRESET por comando */
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    err = esp_lcd_new_panel_st7789(io_handle, &panel_cfg, &s_panel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_lcd_new_panel_st7789 falhou: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_lcd_panel_reset(s_panel);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_lcd_panel_init(s_panel);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_lcd_panel_invert_color(s_panel, true);
    if (err != ESP_OK) {
        return err;
    }
    /* Controlador ST7789 é nativamente 240 (coluna) x 320 (linha) — pra
     * paisagem 320x240 é preciso trocar eixos, senão o draw_bitmap manda
     * endereço de coluna até 319 num controlador de só 240 colunas de RAM,
     * o que corrompe a borda (era isso que causava o artefato nos dois
     * lados, não a velocidade do SPI). */
    err = esp_lcd_panel_swap_xy(s_panel, true);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_lcd_panel_disp_on_off(s_panel, true);
    if (err != ESP_OK) {
        return err;
    }

    ESP_LOGI(TAG, "display_hal inicializado (%ux%u, SPI %d MHz)", NB_DISPLAY_HAL_WIDTH,
             NB_DISPLAY_HAL_HEIGHT, NB_DISPLAY_HAL_PIXEL_CLOCK_HZ / 1000000);
    return ESP_OK;
}

uint16_t *nb_display_hal_shell_get_back_buffer(void)
{
    return nb_display_hal_get_back_buffer(&s_hal);
}

esp_err_t nb_display_hal_shell_flush_and_swap(void)
{
    esp_err_t err;
    uint16_t *back_buffer;

    /* Barreira: espera a transferência DMA anterior terminar antes de enfileirar
     * a próxima e trocar os buffers. Garante uma única transferência em voo e
     * que o buffer prestes a ser reusado não está mais sendo lido pelo DMA. */
    xSemaphoreTake(s_trans_done, portMAX_DELAY);

    back_buffer = nb_display_hal_get_back_buffer(&s_hal);

    /* esp_lcd_panel_io_spi não seta SPI_TRANS_DMA_USE_PSRAM nas transações
     * que monta, então o spi_master não sincroniza cache sozinho pra
     * buffers em PSRAM (ver comentário no topo do arquivo). Sem isso o DMA
     * pode ler dado ainda não escrito de volta da cache -> corrupção de
     * cor, independente de clock ou fiação (confirmado em bancada). */
    err = esp_cache_msync(back_buffer, s_buffer_bytes, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
    if (err != ESP_OK) {
        xSemaphoreGive(s_trans_done);
        return err;
    }

    err = esp_lcd_panel_draw_bitmap(s_panel, 0, 0, NB_DISPLAY_HAL_WIDTH, NB_DISPLAY_HAL_HEIGHT,
                                    back_buffer);
    if (err != ESP_OK) {
        /* Não houve transferência em voo: devolve o token para não travar o
         * próximo flush. */
        xSemaphoreGive(s_trans_done);
        return err;
    }

    nb_display_hal_swap(&s_hal);
    return ESP_OK;
}

void nb_display_hal_shell_draw_test_pattern(uint16_t *buffer, int offset_rows)
{
    static const uint16_t colors[5] = {
        0xF800u, /* vermelho */
        0x07E0u, /* verde */
        0x001Fu, /* azul */
        0xFFFFu, /* branco */
        0x0000u, /* preto */
    };
    const int band_height = (int)NB_DISPLAY_HAL_HEIGHT / 5;

    if (buffer == NULL) {
        return;
    }

    for (int y = 0; y < (int)NB_DISPLAY_HAL_HEIGHT; ++y) {
        int band = (((y + offset_rows) / band_height) % 5 + 5) % 5;
        uint16_t color = colors[band];

        for (int x = 0; x < (int)NB_DISPLAY_HAL_WIDTH; ++x) {
            buffer[(size_t)y * NB_DISPLAY_HAL_WIDTH + (size_t)x] = color;
        }
    }
}
