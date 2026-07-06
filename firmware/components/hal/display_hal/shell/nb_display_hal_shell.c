#include "nb_display_hal_shell.h"

#include "driver/spi_master.h"
#include "esp_attr.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_st7789.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

#define NB_DISPLAY_HAL_SPI_HOST SPI2_HOST
#define NB_DISPLAY_HAL_PIN_SCLK 12
#define NB_DISPLAY_HAL_PIN_MOSI 11
#define NB_DISPLAY_HAL_PIN_CS 10
#define NB_DISPLAY_HAL_PIN_DC 14
/* ESP-IDF/ESP-IoT-Solution documenta que o SPI não transfere PSRAM por DMA
 * direto nesses targets: o driver faz staging PSRAM->SRAM antes do DMA.
 * Bancada 2026-07-06: 80 MHz apresentou artefatos; 60 MHz ficou visualmente
 * normal, mas não melhorou fps, indicando clock efetivo/overhead sem ganho
 * de produção. O default volta ao ponto estável; S4.1a reduz bytes. */
#define NB_DISPLAY_HAL_PIXEL_CLOCK_HZ (40 * 1000 * 1000)

/* S4.1: o flush era um único draw_bitmap de 320x240x2 = 150 KB. Como o
 * staging PSRAM->SRAM é o caminho suportado pelo driver SPI, esse staging
 * também consome SRAM interna DMA-capable. Depois que o I2S (audio_hal)
 * subiu e passou a consumir SRAM do mesmo pool, o staging de 150 KB deixou
 * de caber -> setup_dma_priv_buffer falhava com ESP_ERR_NO_MEM em todo
 * flush. Fatiar em bandas deixa cada staging pequeno (~19 KB @ 30 linhas),
 * mantendo framebuffer em PSRAM e I2S DMA em SRAM. */
#define NB_DISPLAY_HAL_FLUSH_BANDS 8u
#define NB_DISPLAY_HAL_MAX_STAGING_BYTES \
    (((size_t)NB_DISPLAY_HAL_WIDTH * NB_DISPLAY_HAL_HEIGHT * sizeof(uint16_t)) / \
     NB_DISPLAY_HAL_FLUSH_BANDS)

static const char *TAG = "display_hal";
static nb_display_hal_t s_hal;
static esp_lcd_panel_handle_t s_panel;
static uint16_t *s_flush_stage;

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

    /* Mantém os framebuffers alinhados à linha de cache. O flush normal de
     * S4.1a copia dirty rectangles para um staging DMA interno, mas buffers
     * alinhados preservam margem para diagnósticos/full-frame e evitam
     * reabrir a classe de bugs de coerência vista no S2.1. */
    buffer_a = heap_caps_aligned_alloc(32, buffer_bytes, MALLOC_CAP_SPIRAM);
    buffer_b = heap_caps_aligned_alloc(32, buffer_bytes, MALLOC_CAP_SPIRAM);
    if (buffer_a == NULL || buffer_b == NULL) {
        ESP_LOGE(TAG, "falha ao alocar framebuffers em PSRAM (%u bytes cada)",
                 (unsigned)buffer_bytes);
        return ESP_ERR_NO_MEM;
    }
    nb_display_hal_init(&s_hal, buffer_a, buffer_b);

    s_flush_stage = heap_caps_aligned_alloc(32, NB_DISPLAY_HAL_MAX_STAGING_BYTES,
                                            MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    if (s_flush_stage == NULL) {
        ESP_LOGE(TAG, "falha ao alocar staging DMA do display (%u bytes)",
                 (unsigned)NB_DISPLAY_HAL_MAX_STAGING_BYTES);
        return ESP_ERR_NO_MEM;
    }

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
    /* swap_xy sozinho deixa a imagem de cabeça para baixo nesta orientação
     * de montagem -- o padrão de barras horizontais do bring-up (S2.1) não
     * denunciava isso (uma faixa invertida verticalmente ainda parece um
     * conjunto válido de faixas horizontais); só ficou visível com conteúdo
     * assimétrico em cima/embaixo (S2.2, formas dos olhos). Depois do
     * swap_xy (MV=1), os parâmetros mirror_x/mirror_y do esp_lcd mapeiam
     * pros eixos físicos trocados -- testado mirror(false,true) e ainda
     * ficou invertido, então o eixo que precisa espelhar aqui é o outro. */
    err = esp_lcd_panel_mirror(s_panel, true, false);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_lcd_panel_disp_on_off(s_panel, true);
    if (err != ESP_OK) {
        return err;
    }

    ESP_LOGI(TAG, "display_hal inicializado (%ux%u, SPI %d MHz, stage=%u bytes)",
             NB_DISPLAY_HAL_WIDTH, NB_DISPLAY_HAL_HEIGHT,
             NB_DISPLAY_HAL_PIXEL_CLOCK_HZ / 1000000,
             (unsigned)NB_DISPLAY_HAL_MAX_STAGING_BYTES);
    return ESP_OK;
}

uint16_t *nb_display_hal_shell_get_back_buffer(void)
{
    return nb_display_hal_get_back_buffer(&s_hal);
}

static esp_err_t nb_display_hal_shell_flush_aligned_rect_and_swap(nb_display_hal_rect_t rect)
{
    esp_err_t err;
    uint16_t *const back_buffer = nb_display_hal_get_back_buffer(&s_hal);
    const int width = (int)NB_DISPLAY_HAL_WIDTH;
    const int rect_x0 = (int)rect.x;
    const int rect_x1 = (int)rect.x + (int)rect.w;
    const int rect_y1 = (int)rect.y + (int)rect.h;
    const size_t row_bytes = (size_t)rect.w * sizeof(uint16_t);
    size_t max_rows = NB_DISPLAY_HAL_MAX_STAGING_BYTES / row_bytes;

    if (back_buffer == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (max_rows == 0u) {
        max_rows = 1u;
    }

    /* Envia a região em bandas horizontais, serializando banda a banda pelo
     * s_trans_done: uma única transferência (e um único bounce interno) em
     * voo por vez. Cada banda começa em endereço e tem tamanho múltiplos de
     * 32 bytes porque o retângulo foi alinhado em X para 16 pixels RGB565. */
    for (int y0 = (int)rect.y; y0 < rect_y1; y0 += (int)max_rows) {
        int y1 = y0 + (int)max_rows;
        if (y1 > rect_y1) {
            y1 = rect_y1;
        }
        if (s_flush_stage == NULL) {
            return ESP_ERR_INVALID_STATE;
        }

        /* Espera a banda anterior terminar antes de sobrescrever o staging
         * DMA único e enfileirar a próxima. */
        xSemaphoreTake(s_trans_done, portMAX_DELAY);

        for (int y = y0; y < y1; ++y) {
            const uint16_t *const src =
                back_buffer + (size_t)y * (size_t)width + (size_t)rect.x;
            uint16_t *const dst =
                s_flush_stage + (size_t)(y - y0) * (size_t)rect.w;
            memcpy(dst, src, row_bytes);
        }

        err = esp_lcd_panel_draw_bitmap(s_panel, rect_x0, y0, rect_x1, y1, s_flush_stage);
        if (err != ESP_OK) {
            /* Banda não enfileirada: devolve o token para não travar o loop. */
            xSemaphoreGive(s_trans_done);
            return err;
        }
    }

    /* Só troca os buffers depois que a última banda saiu: aí o back buffer
     * não está mais sendo lido pelo DMA. */
    xSemaphoreTake(s_trans_done, portMAX_DELAY);
    nb_display_hal_swap(&s_hal);
    xSemaphoreGive(s_trans_done);
    return ESP_OK;
}

esp_err_t nb_display_hal_shell_flush_rect_and_swap(nb_display_hal_rect_t rect)
{
    rect = nb_display_hal_rect_align_for_flush(rect);
    if (nb_display_hal_rect_is_empty(rect)) {
        return ESP_OK;
    }

    return nb_display_hal_shell_flush_aligned_rect_and_swap(rect);
}

esp_err_t nb_display_hal_shell_flush_and_swap(void)
{
    return nb_display_hal_shell_flush_rect_and_swap(NB_DISPLAY_HAL_RECT_FULL);
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
