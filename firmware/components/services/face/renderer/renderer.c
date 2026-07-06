#include "renderer.h"

#include <math.h>

/*
 * Tabela das 10 expressões-base (VISUAL.md §2), valores herdados do
 * renderer do v1 (nb_head_emo_renderer::kExpressions) para paridade visual
 * -- reescritos aqui na estrutura núcleo/casca do NB2, não copiados do
 * arquivo do v1. Ordem dos campos por linha:
 * tl_l,tr_l,bl_l,br_l, tl_r,tr_r,bl_r,br_r, open_l,open_r, y_l,y_r, x_off,
 * round_top,round_bottom, curve_top,curve_bottom, squint_l,squint_r,
 * mouth_open,mouth_curve.
 *
 * Boca (S3.7 completo, item 5, RFC-VIDA-V2.md §3.1): só os 4 hubs
 * (NEUTRAL/HAPPY/SAD/ANGRY) ganham valor não-neutro -- as outras 6
 * âncoras continuam "boca neutra" (0,0), intocadas, conforme o RFC
 * decidiu explicitamente pra esta fase. Sem variantes/campo contínuo
 * ainda (item 6).
 */
static const nb_face_state_t s_expressions[NB_FACE_EXPR_COUNT] = {
    /* NEUTRAL -- "fechada, micro-curva" (RFC §3.1) */
    {0, 0, 0, 0, 0, 0, 0, 0, .88f, .88f, 0, 0, .60f, .64f, .64f, 0, 0, 0, 0, 0.0f, .05f},
    /* HAPPY -- "sorriso (aberto no nível alto)" */
    {0, 0, .72f, .72f, 0, 0, .72f, .72f, .41f, .41f, 0, 0, .60f, .27f, .52f, 1, -1, .22f, .22f,
     .25f, .70f},
    /* CURIOUS -- boca neutra */
    {0, 0, 0, 0, .19f, 0, 0, 0, .82f, .96f, 0, 0, .60f, .64f, .64f, 0, 0, 0, 0, 0.0f, 0.0f},
    /* SLEEPY -- boca neutra */
    {0, 0, 0, 0, 0, 0, 0, 0, .14f, .14f, -.55f, -.55f, .60f, .19f, 1, -.38f, .45f, .51f, .51f,
     0.0f, 0.0f},
    /* FOCUSED -- boca neutra */
    {0, .30f, 0, 0, .30f, 0, 0, 0, .80f, .80f, 0, 0, .60f, .64f, .64f, .30f, .05f, .10f, .10f,
     0.0f, 0.0f},
    /* SUSPICIOUS -- boca neutra */
    {0, .38f, 0, 0, .38f, 0, 0, 0, .86f, .86f, .10f, .10f, .60f, .64f, .64f, -.44f, .05f, .38f,
     .38f, 0.0f, 0.0f},
    /* SURPRISED -- boca neutra */
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, .60f, .64f, .64f, 0, 0, 0, 0, 0.0f, 0.0f},
    /* SAD -- "curva pra baixo suave" */
    {.70f, .13f, 0, .44f, 0, .70f, .44f, 0, .68f, .68f, .20f, .20f, .60f, .64f, .64f, -.16f, .08f,
     .08f, .08f, 0.0f, -.50f},
    /* ALARMED -- boca neutra */
    {0, .28f, 0, 0, .28f, 0, 0, 0, .88f, .88f, -.18f, -.18f, .60f, .64f, .64f, .55f, .10f, 0, 0,
     0.0f, 0.0f},
    /* ANGRY -- "curva invertida firme" */
    {0, .88f, .93f, .60f, .88f, 0, .60f, .93f, .82f, .82f, 0, 0, .60f, .26f, .14f, .06f, -.10f, 0,
     0, 0.0f, -.70f},
};

const nb_face_state_t *nb_face_core_get_expression(nb_face_expr_t expr)
{
    const uint32_t idx = ((uint32_t)expr < NB_FACE_EXPR_COUNT) ? (uint32_t)expr : 0u;
    return &s_expressions[idx];
}

static float nb_face_lerpf(float a, float b, float t)
{
    return a + (b - a) * t;
}

void nb_face_core_lerp(const nb_face_state_t *a, const nb_face_state_t *b, float t,
                       nb_face_state_t *out)
{
    out->tl_l = nb_face_lerpf(a->tl_l, b->tl_l, t);
    out->tr_l = nb_face_lerpf(a->tr_l, b->tr_l, t);
    out->bl_l = nb_face_lerpf(a->bl_l, b->bl_l, t);
    out->br_l = nb_face_lerpf(a->br_l, b->br_l, t);
    out->tl_r = nb_face_lerpf(a->tl_r, b->tl_r, t);
    out->tr_r = nb_face_lerpf(a->tr_r, b->tr_r, t);
    out->bl_r = nb_face_lerpf(a->bl_r, b->bl_r, t);
    out->br_r = nb_face_lerpf(a->br_r, b->br_r, t);
    out->open_l = nb_face_lerpf(a->open_l, b->open_l, t);
    out->open_r = nb_face_lerpf(a->open_r, b->open_r, t);
    out->y_l = nb_face_lerpf(a->y_l, b->y_l, t);
    out->y_r = nb_face_lerpf(a->y_r, b->y_r, t);
    out->x_off = nb_face_lerpf(a->x_off, b->x_off, t);
    out->round_top = nb_face_lerpf(a->round_top, b->round_top, t);
    out->round_bottom = nb_face_lerpf(a->round_bottom, b->round_bottom, t);
    out->curve_top = nb_face_lerpf(a->curve_top, b->curve_top, t);
    out->curve_bottom = nb_face_lerpf(a->curve_bottom, b->curve_bottom, t);
    out->squint_l = nb_face_lerpf(a->squint_l, b->squint_l, t);
    out->squint_r = nb_face_lerpf(a->squint_r, b->squint_r, t);
    out->mouth_open = nb_face_lerpf(a->mouth_open, b->mouth_open, t);
    out->mouth_curve = nb_face_lerpf(a->mouth_curve, b->mouth_curve, t);
}

float nb_face_core_clampf(float v, float lo, float hi)
{
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

/* Constante de curvatura da parábola central (v1: kCurve). */
#define NB_FACE_CURVE_PX 10.0f

void nb_face_core_eye_column(float open, float tl, float tr, float bl, float br, float squint,
                             float round_top, float round_bottom, float curve_top,
                             float curve_bottom, int16_t half_width, float cy, int16_t x_rel,
                             nb_face_eye_column_t *out)
{
    out->eye_closed = false;
    out->has_top_aa = false;
    out->has_bottom_aa = false;
    out->alpha_top = 0.0f;
    out->alpha_bottom = 0.0f;
    out->top_full = 1;
    out->bottom_full = 0; /* faixa vazia por padrão (nada a desenhar) */

    if (open < .03f) {
        out->eye_closed = true;
        return;
    }

    const float hh = open * NB_FACE_EYE_HALF_HEIGHT;
    const float y_tl = cy - hh * (1.0f - tl);
    const float y_tr = cy - hh * (1.0f - tr);
    const float y_bl = cy + hh * (1.0f - bl);
    const float y_br = cy + hh * (1.0f - br);
    const float squint_px = squint * hh * .65f;
    const float r_top = fminf(round_top * (float)half_width * .68f, hh);
    const float r_bottom = fminf(round_bottom * (float)half_width * .68f, hh);

    const float span = (float)half_width * 2.0f;
    const float t = (float)x_rel / span;
    const float parabola = 4.0f * t * (1.0f - t);
    float top = y_tl + t * (y_tr - y_tl) - curve_top * NB_FACE_CURVE_PX * parabola;
    float bottom = y_bl + t * (y_br - y_bl) + curve_bottom * NB_FACE_CURVE_PX * parabola;
    const float edge = fminf((float)x_rel, span - (float)x_rel);

    if (r_top > 0.0f && edge < r_top) {
        const float q = 1.0f - edge / r_top;
        top += r_top * (1.0f - sqrtf(1.0f - q * q));
    }
    if (r_bottom > 0.0f && edge < r_bottom) {
        const float q = 1.0f - edge / r_bottom;
        bottom -= r_bottom * (1.0f - sqrtf(1.0f - q * q));
    }

    top += squint_px;
    if (top >= bottom - .5f) {
        return; /* faixa vazia: squint fechou o olho nesta coluna */
    }

    const int16_t top_full = (int16_t)ceilf(top);
    const int16_t bottom_full = (int16_t)floorf(bottom);
    const float alpha_top = (float)top_full - top;
    const float alpha_bottom = bottom - (float)bottom_full;

    out->top_full = top_full;
    out->bottom_full = bottom_full;
    if (alpha_top > .04f) {
        out->has_top_aa = true;
        out->alpha_top = alpha_top;
    }
    if (alpha_bottom > .04f) {
        out->has_bottom_aa = true;
        out->alpha_bottom = alpha_bottom;
    }
}

void nb_face_core_mouth_column(float mouth_open, float mouth_curve, int16_t half_width, float cy,
                               int16_t x_rel, nb_face_mouth_column_t *out)
{
    const float span = (float)half_width * 2.0f;
    const float t = (float)x_rel / span;
    const float parabola = 4.0f * t * (1.0f - t); /* 0 nas pontas, 1 no centro */
    /* mouth_curve > 0: pontas sobem em relação ao centro (sorriso, "⌣").
     * mouth_curve < 0: pontas descem (franzido, "⌢"). */
    const float center_y = cy - mouth_curve * NB_FACE_MOUTH_CURVE_PX * (1.0f - parabola);
    /* Nunca some de vez -- mesmo mouth_open=0 precisa mostrar a curvatura
     * (micro-sorriso do NEUTRAL, franzido de SAD/ANGRY com boca fechada). */
    const float half_h = fmaxf(mouth_open * NB_FACE_MOUTH_HALF_HEIGHT, 0.5f);

    const float top = center_y - half_h;
    const float bottom = center_y + half_h;
    const int16_t top_full = (int16_t)ceilf(top);
    const int16_t bottom_full = (int16_t)floorf(bottom);
    const float alpha_top = (float)top_full - top;
    const float alpha_bottom = bottom - (float)bottom_full;

    out->top_full = top_full;
    out->bottom_full = bottom_full;
    out->has_top_aa = alpha_top > .04f;
    out->alpha_top = out->has_top_aa ? alpha_top : 0.0f;
    out->has_bottom_aa = alpha_bottom > .04f;
    out->alpha_bottom = out->has_bottom_aa ? alpha_bottom : 0.0f;
}

void nb_face_core_blend(const nb_face_state_t *const *states, const float *weights,
                        uint32_t count, nb_face_state_t *out)
{
    if (out == NULL || states == NULL || weights == NULL || count == 0u) {
        return;
    }

#define NB_FACE_BLEND_FIELD(field)                                                              \
    do {                                                                                        \
        float acc = 0.0f;                                                                       \
        for (uint32_t i = 0; i < count; ++i) {                                                  \
            acc += weights[i] * states[i]->field;                                               \
        }                                                                                        \
        out->field = acc;                                                                       \
    } while (0)

    NB_FACE_BLEND_FIELD(tl_l);
    NB_FACE_BLEND_FIELD(tr_l);
    NB_FACE_BLEND_FIELD(bl_l);
    NB_FACE_BLEND_FIELD(br_l);
    NB_FACE_BLEND_FIELD(tl_r);
    NB_FACE_BLEND_FIELD(tr_r);
    NB_FACE_BLEND_FIELD(bl_r);
    NB_FACE_BLEND_FIELD(br_r);
    NB_FACE_BLEND_FIELD(open_l);
    NB_FACE_BLEND_FIELD(open_r);
    NB_FACE_BLEND_FIELD(y_l);
    NB_FACE_BLEND_FIELD(y_r);
    NB_FACE_BLEND_FIELD(x_off);
    NB_FACE_BLEND_FIELD(round_top);
    NB_FACE_BLEND_FIELD(round_bottom);
    NB_FACE_BLEND_FIELD(curve_top);
    NB_FACE_BLEND_FIELD(curve_bottom);
    NB_FACE_BLEND_FIELD(squint_l);
    NB_FACE_BLEND_FIELD(squint_r);
    NB_FACE_BLEND_FIELD(mouth_open);
    NB_FACE_BLEND_FIELD(mouth_curve);

#undef NB_FACE_BLEND_FIELD
}
