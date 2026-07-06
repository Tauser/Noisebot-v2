#ifndef NB_BREATH_H
#define NB_BREATH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Núcleo C17 puro -- respiração do idle v2 (S3.7 spike, docs/RFC-VIDA-V2.md
 * §7, motor 3 "Energia" / fisiologia). Fator multiplicativo de altura/
 * abertura dos olhos; período ~6s, amplitude 1.5-2.5% de escala vertical
 * (RFC §7). Sem estado: função pura de now_ms, sem RNG (a respiração não é
 * estocástica).
 */

#define NB_BREATH_PERIOD_MS_DEFAULT 6000u
#define NB_BREATH_AMPLITUDE_DEFAULT 0.02f /* 2%, dentro da faixa 1.5-2.5% do RFC */

/* Fator (1 +- amp) para multiplicar sobre a abertura/altura dos olhos.
 * period_ms/amp expostos para tuning e host-test; produção usa os
 * *_DEFAULT acima. period_ms == 0 retorna 1.0 (sem modulação). */
float nb_breath_scale(uint32_t now_ms, uint32_t period_ms, float amp);

#ifdef __cplusplus
}
#endif

#endif
