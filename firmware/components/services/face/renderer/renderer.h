#ifndef NB_FACE_CORE_H
#define NB_FACE_CORE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Núcleo C17 puro do renderer de face (S2.2).
 *
 * Sem LovyanGFX, sem FreeRTOS, sem ESP-IDF: só o estado paramétrico da face
 * (VISUAL.md §1), a tabela das 10 expressões-base (VISUAL.md §2, valores
 * herdados do renderer do v1 para paridade visual) e a matemática de
 * interpolação/geometria por coluna usada no anti-aliasing sub-pixel. A
 * casca (shell/, C++) só chama estas funções e escreve os pixels resultantes
 * no LGFX_Sprite.
 */

#define NB_FACE_EXPR_COUNT 10u
#define NB_FACE_EYE_HALF_WIDTH 46
#define NB_FACE_EYE_HALF_HEIGHT 46.0f

typedef enum {
    NB_FACE_EXPR_NEUTRAL = 0,
    NB_FACE_EXPR_HAPPY = 1,
    NB_FACE_EXPR_CURIOUS = 2,
    NB_FACE_EXPR_SLEEPY = 3,
    NB_FACE_EXPR_FOCUSED = 4,
    NB_FACE_EXPR_SUSPICIOUS = 5,
    NB_FACE_EXPR_SURPRISED = 6,
    NB_FACE_EXPR_SAD = 7,
    NB_FACE_EXPR_ALARMED = 8,
    NB_FACE_EXPR_ANGRY = 9,
} nb_face_expr_t;

/* Estado paramétrico completo de uma face (VISUAL.md §1), por olho L/R. */
typedef struct {
    float tl_l, tr_l, bl_l, br_l;
    float tl_r, tr_r, bl_r, br_r;
    float open_l, open_r;
    float y_l, y_r;
    float x_off;
    float round_top, round_bottom;
    float curve_top, curve_bottom;
    float squint_l, squint_r;
} nb_face_state_t;

/* Tabela canônica indexada por nb_face_expr_t. expr fora do intervalo
 * retorna NEUTRAL (fallback seguro). */
const nb_face_state_t *nb_face_core_get_expression(nb_face_expr_t expr);

/* Interpolação linear campo-a-campo, t em [0,1] (não clampado pelo chamador
 * -- o chamador do shell garante 0<=t<=1 pelo tick de 220 ms). */
void nb_face_core_lerp(const nb_face_state_t *a, const nb_face_state_t *b, float t,
                       nb_face_state_t *out);

float nb_face_core_clampf(float v, float lo, float hi);

/* Geometria vertical (com AA sub-pixel) de uma coluna de um olho, calculada
 * como no v1: parábola de curvatura + squint + arredondamento de canto por
 * distância à borda lateral. x_rel é relativo à borda esquerda do olho
 * (0..2*half_width). */
typedef struct {
    bool eye_closed; /* true: desenhar barra horizontal, ignorar os outros campos */
    int16_t top_full;     /* primeira linha cheia (sem AA) */
    int16_t bottom_full;   /* última linha cheia (sem AA), inclusive */
    bool has_top_aa;
    float alpha_top;      /* alpha do pixel em top_full - 1 */
    bool has_bottom_aa;
    float alpha_bottom;   /* alpha do pixel em bottom_full + 1 */
} nb_face_eye_column_t;

void nb_face_core_eye_column(float open, float tl, float tr, float bl, float br, float squint,
                             float round_top, float round_bottom, float curve_top,
                             float curve_bottom, int16_t half_width, float cy, int16_t x_rel,
                             nb_face_eye_column_t *out);

#ifdef __cplusplus
}
#endif

#endif
