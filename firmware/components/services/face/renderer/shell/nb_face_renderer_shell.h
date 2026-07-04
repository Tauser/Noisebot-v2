#ifndef NB_FACE_RENDERER_SHELL_H
#define NB_FACE_RENDERER_SHELL_H

#include <stdint.h>

#include "display_hal.h"
#include "esp_err.h"
#include "renderer.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Casca C++ (LovyanGFX) do renderer de face (S2.2), atrás de extern "C" —
 * único ponto do firmware fora de renderer/ que instancia um LGFX_Sprite
 * (`ARCHITECTURE.md` §5: "nenhum outro componente toca o objeto LGFX").
 *
 * Não aloca framebuffer próprio: o LGFX_Sprite é amarrado (setBuffer) ao
 * back buffer do display_hal (S2.1), reaproveitando o double buffer já em
 * PSRAM.
 */

/* Prepara o LGFX_Sprite amarrado ao buffer informado (deve ter
 * NB_DISPLAY_HAL_WIDTH*NB_DISPLAY_HAL_HEIGHT pixels RGB565). Chamar de novo
 * após cada nb_display_hal_shell_flush_and_swap() para religar no novo back
 * buffer. */
esp_err_t nb_face_renderer_shell_bind_buffer(uint16_t *buffer);

/* Desenha o estado de face já resolvido (estático ou interpolado por
 * nb_face_core_lerp) no buffer amarrado. gaze_x/gaze_y em [-1, 1]. */
void nb_face_renderer_shell_draw(const nb_face_state_t *face, float gaze_x, float gaze_y,
                                 uint32_t color);

#ifdef __cplusplus
}
#endif

#endif
