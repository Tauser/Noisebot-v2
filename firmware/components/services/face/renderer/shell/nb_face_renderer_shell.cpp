#include "nb_face_renderer_shell.h"

#include <LovyanGFX.hpp>

#include "esp_log.h"

namespace {

/* Geometria da face (pixels, canvas 320x240 landscape) -- herdada do v1
 * (nb_head_emo_renderer) para paridade visual, ver `VISUAL.md` §1/§2. */
constexpr int16_t kBaseLeftX = 96;
constexpr int16_t kBaseRightX = 224;
constexpr int16_t kEyeBaseY = 122;
/* kYTravel/kGazeXTravel retunados em bancada (2026-07-04, junto do S2.4):
 * os valores herdados do v1 (32/14) mapeavam o gaze de idle_engine pra
 * poucos pixels (<18px mesmo no limite) -- imperceptível/curto demais
 * neste display. Aumentados dentro da folga real de tela (eyes em
 * kBaseLeftX=96/kBaseRightX=224, meia-largura 46px -> ~50px de margem
 * até a borda de cada lado; verticalmente ~72-76px de folga acima/abaixo
 * de kEyeBaseY). */
constexpr float kYTravel = 55.0f;
constexpr float kXOffsetTravel = 18.0f;
constexpr float kGazeXTravel = 26.0f;
constexpr float kGazeYLimit = .55f;
/* Deslocamento vertical por unidade de tilt (HEAD_TILT_HOLD, S2.4:
 * assimetria ±0.10 de VISUAL.md §3) -- ponto de partida, mesma lição do
 * gaze (kGazeXTravel/kYTravel): valor a confirmar/ajustar em bancada. */
constexpr float kTiltTravel = 80.0f;

const char *TAG = "face_renderer";

LGFX_Sprite s_sprite;
bool s_bound = false;
bool s_has_last_dirty = false;
nb_display_hal_rect_t s_last_dirty = {0, 0, 0, 0};

uint32_t blend_black(uint32_t color, float alpha)
{
    const uint8_t r = static_cast<uint8_t>(((color >> 16) & 0xffU) * alpha);
    const uint8_t g = static_cast<uint8_t>(((color >> 8) & 0xffU) * alpha);
    const uint8_t b = static_cast<uint8_t>((color & 0xffU) * alpha);
    return (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | b;
}

void draw_eye(int16_t cx, float cy, float open, float tl, float tr, float bl, float br,
             float squint, float round_top, float round_bottom, float curve_top,
             float curve_bottom, int16_t half_width, uint32_t color)
{
    nb_face_eye_column_t col;

    nb_face_core_eye_column(open, tl, tr, bl, br, squint, round_top, round_bottom, curve_top,
                            curve_bottom, half_width, cy, 0, &col);
    if (col.eye_closed) {
        s_sprite.drawFastHLine(cx - half_width, static_cast<int16_t>(cy), half_width * 2, color);
        return;
    }

    for (int16_t x_rel = 0; x_rel <= half_width * 2; ++x_rel) {
        nb_face_core_eye_column(open, tl, tr, bl, br, squint, round_top, round_bottom, curve_top,
                                curve_bottom, half_width, cy, x_rel, &col);

        const int16_t x = static_cast<int16_t>(cx - half_width + x_rel);

        if (col.has_top_aa) {
            s_sprite.drawPixel(x, col.top_full - 1, blend_black(color, col.alpha_top));
        }
        if (col.bottom_full >= col.top_full) {
            s_sprite.drawFastVLine(x, col.top_full, col.bottom_full - col.top_full + 1, color);
        }
        if (col.has_bottom_aa) {
            s_sprite.drawPixel(x, col.bottom_full + 1, blend_black(color, col.alpha_bottom));
        }
    }
}

nb_display_hal_rect_t rect_from_bounds(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    if (x0 < 0) {
        x0 = 0;
    }
    if (y0 < 0) {
        y0 = 0;
    }
    if (x1 > static_cast<int16_t>(NB_DISPLAY_HAL_WIDTH)) {
        x1 = static_cast<int16_t>(NB_DISPLAY_HAL_WIDTH);
    }
    if (y1 > static_cast<int16_t>(NB_DISPLAY_HAL_HEIGHT)) {
        y1 = static_cast<int16_t>(NB_DISPLAY_HAL_HEIGHT);
    }
    if (x1 <= x0 || y1 <= y0) {
        return {0, 0, 0, 0};
    }

    return {static_cast<uint16_t>(x0), static_cast<uint16_t>(y0),
            static_cast<uint16_t>(x1 - x0), static_cast<uint16_t>(y1 - y0)};
}

nb_display_hal_rect_t eye_dirty_rect(int16_t cx, float cy, int16_t half_width)
{
    constexpr int16_t kPadX = 3;
    constexpr int16_t kPadY = 16;
    const int16_t half_height = static_cast<int16_t>(NB_FACE_EYE_HALF_HEIGHT) + kPadY;
    const int16_t y_center = static_cast<int16_t>(cy + ((cy >= 0.0f) ? 0.5f : -0.5f));

    return rect_from_bounds(static_cast<int16_t>(cx - half_width - kPadX),
                            static_cast<int16_t>(y_center - half_height),
                            static_cast<int16_t>(cx + half_width + kPadX + 1),
                            static_cast<int16_t>(y_center + half_height + 1));
}

nb_display_hal_rect_t face_dirty_rect(int16_t left_x, float left_y, int16_t half_width_l,
                                      int16_t right_x, float right_y, int16_t half_width_r)
{
    const nb_display_hal_rect_t left = eye_dirty_rect(left_x, left_y, half_width_l);
    const nb_display_hal_rect_t right = eye_dirty_rect(right_x, right_y, half_width_r);
    return nb_display_hal_rect_union(left, right);
}

} // namespace

esp_err_t nb_face_renderer_shell_bind_buffer(uint16_t *buffer)
{
    if (buffer == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    s_sprite.setPsram(false); /* buffer já é externo (PSRAM do display_hal) */
    s_sprite.setBuffer(buffer, NB_DISPLAY_HAL_WIDTH, NB_DISPLAY_HAL_HEIGHT, 16);
    s_bound = true;
    return ESP_OK;
}

void nb_face_renderer_shell_draw(const nb_face_state_t *face, float gaze_x, float gaze_y,
                                 float width_l, float width_r, float tilt, uint32_t color)
{
    (void)nb_face_renderer_shell_draw_dirty(face, gaze_x, gaze_y, width_l, width_r, tilt, color);
}

nb_display_hal_rect_t nb_face_renderer_shell_draw_dirty(const nb_face_state_t *face,
                                                        float gaze_x, float gaze_y,
                                                        float width_l, float width_r,
                                                        float tilt, uint32_t color)
{
    if (face == NULL || !s_bound) {
        ESP_LOGE(TAG, "draw chamado sem face ou sem buffer amarrado");
        return {0, 0, 0, 0};
    }

    const float clamped_gaze_x = nb_face_core_clampf(gaze_x, -1.0f, 1.0f);
    const float clamped_gaze_y = nb_face_core_clampf(gaze_y, -kGazeYLimit, kGazeYLimit);
    const int16_t gaze_shift = static_cast<int16_t>(clamped_gaze_x * kGazeXTravel + 0.5f);
    const int16_t x_offset = static_cast<int16_t>(face->x_off * kXOffsetTravel + 0.5f);
    const int16_t left_x = static_cast<int16_t>(kBaseLeftX + x_offset + gaze_shift);
    const int16_t right_x = static_cast<int16_t>(kBaseRightX - x_offset + gaze_shift);
    /* HEAD_TILT_HOLD (VISUAL.md §3): assimetria vertical -- um olho sobe,
     * o outro desce, simulando roll de cabeça. */
    const float tilt_px = tilt * kTiltTravel;
    const float left_y = kEyeBaseY + (1.0f - face->open_l) * NB_FACE_EYE_HALF_HEIGHT +
                         nb_face_core_clampf(face->y_l + clamped_gaze_y, -kGazeYLimit,
                                            kGazeYLimit) *
                             kYTravel +
                         tilt_px;
    const float right_y = kEyeBaseY + (1.0f - face->open_r) * NB_FACE_EYE_HALF_HEIGHT +
                          nb_face_core_clampf(face->y_r + clamped_gaze_y, -kGazeYLimit,
                                             kGazeYLimit) *
                              kYTravel -
                          tilt_px;
    const int16_t half_width_l =
        static_cast<int16_t>(NB_FACE_EYE_HALF_WIDTH * nb_face_core_clampf(width_l, 0.5f, 1.5f));
    const int16_t half_width_r =
        static_cast<int16_t>(NB_FACE_EYE_HALF_WIDTH * nb_face_core_clampf(width_r, 0.5f, 1.5f));
    const nb_display_hal_rect_t current_dirty =
        face_dirty_rect(left_x, left_y, half_width_l, right_x, right_y, half_width_r);
    const nb_display_hal_rect_t dirty =
        s_has_last_dirty ? nb_display_hal_rect_union(s_last_dirty, current_dirty)
                         : nb_display_hal_rect_t{
                               0, 0, static_cast<uint16_t>(NB_DISPLAY_HAL_WIDTH),
                               static_cast<uint16_t>(NB_DISPLAY_HAL_HEIGHT)};

    s_sprite.startWrite();
    s_sprite.fillScreen(0x000000U);
    draw_eye(left_x, left_y, face->open_l, face->tl_l, face->tr_l, face->bl_l, face->br_l,
             face->squint_l, face->round_top, face->round_bottom, face->curve_top,
             face->curve_bottom, half_width_l, color);
    draw_eye(right_x, right_y, face->open_r, face->tl_r, face->tr_r, face->bl_r, face->br_r,
             face->squint_r, face->round_top, face->round_bottom, face->curve_top,
             face->curve_bottom, half_width_r, color);
    s_sprite.endWrite();

    s_last_dirty = current_dirty;
    s_has_last_dirty = true;
    return dirty;
}
