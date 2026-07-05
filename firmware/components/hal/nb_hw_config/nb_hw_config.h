#ifndef NB_HW_CONFIG_H
#define NB_HW_CONFIG_H

/*
 * Constantes de hardware do NoiseBot 2 (CLAUDE.md §Código: "Constantes de
 * hardware apenas em firmware/components/hal/nb_hw_config.h -- nunca
 * hardcoded em lógica"). Pinout congelado na tag pinout-v1.0 (S0.4);
 * mudança de pino depois disso exige RFC no ROADMAP.
 *
 * Header-only -- sem .c, sem dependência de ESP-IDF/FreeRTOS.
 */

/* touch_hal (S3.1): pad de cobre em GPIO2, canal de touch nativo 2. */
#define NB_HW_GPIO_TOUCH2 2u
#define NB_HW_TOUCH_CHANNEL 2u

/* led_hal (S3.3): 2x WS2812 externos em cadeia, RMT (HARDWARE.md). */
#define NB_HW_GPIO_WS2812 21u
#define NB_HW_LED_COUNT 2u

/* LED de status onboard (HARDWARE.md) -- fixo na placa, não expressivo,
 * fora do escopo de led_service. Reservado aqui pra nenhum componente
 * futuro hardcodar o pino se vier a usá-lo. */
#define NB_HW_GPIO_LED_STATUS_ONBOARD 38u

#endif
