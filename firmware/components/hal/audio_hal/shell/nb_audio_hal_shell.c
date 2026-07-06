#include "nb_audio_hal_shell.h"

#include "driver/i2s_std.h"
#include "esp_log.h"
#include "nb_hw_config.h"

/* Default do driver é dma_desc_num=6/dma_frame_num=240 por canal -- TX+RX
 * juntos consomem ~11,5KB de SRAM interna (DMA-capable), o mesmo pool que
 * o SPI do display_hal usa pros próprios buffers de transferência.
 * Reduzido pra caber os dois com folga. 256 frames = 16ms de áudio a
 * 16kHz por descritor, casando com o chunk de leitura/escrita de 256
 * amostras do bring-up (main.c) -- a causa raiz real do bug de bancada
 * (N32R16V, 2026-07-06) era descompasso de *taxa de alimentação* do TX
 * (escrita menor que o consumo por loop), não o tamanho do buffer em si;
 * esse valor aqui só precisa caber os dois periféricos, não é o que
 * resolve o starvation. */
#define NB_AUDIO_HAL_DMA_DESC_NUM 4u
#define NB_AUDIO_HAL_DMA_FRAME_NUM 256u

static const char *TAG = "audio_hal";
static i2s_chan_handle_t s_tx_handle;
static i2s_chan_handle_t s_rx_handle;
static bool s_initialized;
static volatile uint32_t s_rx_overflow_count;
static volatile uint32_t s_tx_overflow_count;
static volatile uint32_t s_rx_timeout_count;
static volatile uint32_t s_tx_timeout_count;

static void nb_audio_hal_shell_reset_state(void) {
    s_tx_handle = NULL;
    s_rx_handle = NULL;
    s_initialized = false;
}

static void nb_audio_hal_shell_cleanup_partial_init(void) {
    if (s_tx_handle != NULL) {
        i2s_channel_disable(s_tx_handle);
    }
    if (s_rx_handle != NULL) {
        i2s_channel_disable(s_rx_handle);
    }
    if (s_tx_handle != NULL || s_rx_handle != NULL) {
        i2s_del_channel(s_tx_handle);
        s_tx_handle = NULL;
        s_rx_handle = NULL;
    }
}

static bool IRAM_ATTR on_send_q_ovf(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx) {
    (void)handle;
    (void)event;
    (void)user_ctx;
    s_tx_overflow_count++;
    return false;
}

static bool IRAM_ATTR on_recv_q_ovf(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx) {
    (void)handle;
    (void)event;
    (void)user_ctx;
    s_rx_overflow_count++;
    return false;
}

esp_err_t nb_audio_hal_shell_init(void) {
    if (s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    nb_audio_hal_shell_reset_state();
    s_rx_overflow_count = 0u;
    s_tx_overflow_count = 0u;
    s_rx_timeout_count = 0u;
    s_tx_timeout_count = 0u;

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = NB_AUDIO_HAL_DMA_DESC_NUM;
    chan_cfg.dma_frame_num = NB_AUDIO_HAL_DMA_FRAME_NUM;
    esp_err_t err = i2s_new_channel(&chan_cfg, &s_tx_handle, &s_rx_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s_new_channel falhou: %s", esp_err_to_name(err));
        return err;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(NB_HW_AUDIO_SAMPLE_RATE_HZ),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg =
            {
                .mclk = I2S_GPIO_UNUSED,
                .bclk = (gpio_num_t)NB_HW_GPIO_I2S_BCLK,
                .ws = (gpio_num_t)NB_HW_GPIO_I2S_WS,
                .dout = (gpio_num_t)NB_HW_GPIO_I2S_DOUT,
                .din = I2S_GPIO_UNUSED,
                .invert_flags =
                    {
                        .mclk_inv = false,
                        .bclk_inv = false,
                        .ws_inv = false,
                    },
            },
    };
    /* ESP32-S3: a macro de slot mono fixa slot_mask=BOTH independente do
     * argumento -- sobrescreve explicitamente pro canal esquerdo do
     * INMP441 (HARDWARE.md). */
    std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;

    err = i2s_channel_init_std_mode(s_tx_handle, &std_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "init TX falhou: %s", esp_err_to_name(err));
        nb_audio_hal_shell_cleanup_partial_init();
        return err;
    }

    std_cfg.gpio_cfg.dout = I2S_GPIO_UNUSED;
    std_cfg.gpio_cfg.din = (gpio_num_t)NB_HW_GPIO_I2S_DIN;
    err = i2s_channel_init_std_mode(s_rx_handle, &std_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "init RX falhou: %s", esp_err_to_name(err));
        nb_audio_hal_shell_cleanup_partial_init();
        return err;
    }

    i2s_event_callbacks_t tx_callbacks = {.on_send_q_ovf = on_send_q_ovf};
    err = i2s_channel_register_event_callback(s_tx_handle, &tx_callbacks, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "callback TX falhou: %s", esp_err_to_name(err));
        nb_audio_hal_shell_cleanup_partial_init();
        return err;
    }
    i2s_event_callbacks_t rx_callbacks = {.on_recv_q_ovf = on_recv_q_ovf};
    err = i2s_channel_register_event_callback(s_rx_handle, &rx_callbacks, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "callback RX falhou: %s", esp_err_to_name(err));
        nb_audio_hal_shell_cleanup_partial_init();
        return err;
    }

    err = i2s_channel_enable(s_tx_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "enable TX falhou: %s", esp_err_to_name(err));
        nb_audio_hal_shell_cleanup_partial_init();
        return err;
    }
    err = i2s_channel_enable(s_rx_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "enable RX falhou: %s", esp_err_to_name(err));
        nb_audio_hal_shell_cleanup_partial_init();
        return err;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "audio_hal OK -- I2S_NUM_0 full-duplex %u Hz, BCLK%d WS%d DIN%d DOUT%d",
             (unsigned)NB_HW_AUDIO_SAMPLE_RATE_HZ, (int)NB_HW_GPIO_I2S_BCLK, (int)NB_HW_GPIO_I2S_WS,
             (int)NB_HW_GPIO_I2S_DIN, (int)NB_HW_GPIO_I2S_DOUT);
    return ESP_OK;
}

esp_err_t nb_audio_hal_shell_read(int16_t *out_samples, size_t sample_count, uint32_t timeout_ms,
                                  size_t *out_read_count) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    size_t bytes_read = 0;
    esp_err_t err =
        i2s_channel_read(s_rx_handle, out_samples, sample_count * sizeof(int16_t), &bytes_read, timeout_ms);
    if (err == ESP_ERR_TIMEOUT) {
        s_rx_timeout_count++;
    }
    if (out_read_count) {
        *out_read_count = bytes_read / sizeof(int16_t);
    }
    return err;
}

esp_err_t nb_audio_hal_shell_write(const int16_t *samples, size_t sample_count, uint32_t timeout_ms,
                                   size_t *out_written_count) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    size_t bytes_written = 0;
    esp_err_t err =
        i2s_channel_write(s_tx_handle, samples, sample_count * sizeof(int16_t), &bytes_written, timeout_ms);
    if (err == ESP_ERR_TIMEOUT) {
        s_tx_timeout_count++;
    }
    if (out_written_count) {
        *out_written_count = bytes_written / sizeof(int16_t);
    }
    return err;
}

uint32_t nb_audio_hal_shell_get_rx_overflow_count(void) {
    return s_rx_overflow_count;
}

uint32_t nb_audio_hal_shell_get_tx_overflow_count(void) {
    return s_tx_overflow_count;
}

uint32_t nb_audio_hal_shell_get_rx_timeout_count(void) {
    return s_rx_timeout_count;
}

uint32_t nb_audio_hal_shell_get_tx_timeout_count(void) {
    return s_tx_timeout_count;
}
