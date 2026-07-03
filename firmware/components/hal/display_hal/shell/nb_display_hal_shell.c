#include "nb_display_hal_shell.h"

#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_st7789.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"

#define NB_DISPLAY_HAL_SPI_HOST SPI2_HOST
#define NB_DISPLAY_HAL_PIN_SCLK 12
#define NB_DISPLAY_HAL_PIN_MOSI 11
#define NB_DISPLAY_HAL_PIN_CS 10
#define NB_DISPLAY_HAL_PIN_DC 14
/* Mesmo com swap_xy corrigido (resolveu o artefato nas bordas) e sem
 * flicker, 40 MHz ainda mostrou cores erradas (vazamento entre canais:
 * vermelho puro virando laranja, azul virando roxo) -- sinal de bit
 * corrompido na fiação de jumper. Baixando pra validar se 20 MHz elimina
 * de vez; registrar o teto real em HARDWARE.md quando o pinout congelar
 * (S0.4). */
#define NB_DISPLAY_HAL_PIXEL_CLOCK_HZ (20 * 1000 * 1000)

static const char *TAG = "display_hal";
static nb_display_hal_t s_hal;
static esp_lcd_panel_handle_t s_panel;

esp_err_t nb_display_hal_shell_init(void)
{
    const size_t buffer_pixels = (size_t)NB_DISPLAY_HAL_WIDTH * NB_DISPLAY_HAL_HEIGHT;
    const size_t buffer_bytes = buffer_pixels * sizeof(uint16_t);
    uint16_t *buffer_a;
    uint16_t *buffer_b;
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_err_t err;

    buffer_a = heap_caps_malloc(buffer_bytes, MALLOC_CAP_SPIRAM);
    buffer_b = heap_caps_malloc(buffer_bytes, MALLOC_CAP_SPIRAM);
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
    esp_err_t err = esp_lcd_panel_draw_bitmap(s_panel, 0, 0, NB_DISPLAY_HAL_WIDTH,
                                              NB_DISPLAY_HAL_HEIGHT,
                                              nb_display_hal_get_back_buffer(&s_hal));
    if (err != ESP_OK) {
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
