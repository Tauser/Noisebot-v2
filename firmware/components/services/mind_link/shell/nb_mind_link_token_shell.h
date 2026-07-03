#ifndef NB_MIND_LINK_TOKEN_SHELL_H
#define NB_MIND_LINK_TOKEN_SHELL_H

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Persistência do token NBP/2 em NVS (S1.7 — SECURITY.md §"Token").
 *
 * Namespace próprio ("nbp2_tok"), nunca logado. O token é o mesmo campo
 * `bytes(max: 32)` de HELLO em protocol/nbp2.yaml — este componente só
 * guarda os bytes brutos, a validação/comparação timing-safe é do
 * `nbp2_timing_safe_equal` gerado.
 */

#define NB_MIND_LINK_TOKEN_MAX_LEN 32u

esp_err_t nb_mind_link_token_shell_init(void);

/* Copia o token atual para out_token (cap >= NB_MIND_LINK_TOKEN_MAX_LEN).
 * Retorna o comprimento copiado; 0 se nunca foi configurado. */
size_t nb_mind_link_token_shell_get(uint8_t *out_token, size_t out_cap);

/* Persiste um token novo (0..NB_MIND_LINK_TOKEN_MAX_LEN bytes). */
esp_err_t nb_mind_link_token_shell_set(const uint8_t *token, size_t token_len);

#ifdef __cplusplus
}
#endif

#endif
