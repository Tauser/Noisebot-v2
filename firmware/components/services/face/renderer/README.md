# renderer (face paramétrica)

Componente `firmware/components/services/face/renderer` — S2.2. Núcleo C17
puro (`renderer.c/.h`): `nb_face_state_t`, tabela das 10 expressões-base de
`VISUAL.md` §2 (valores herdados do v1 para paridade visual) e
`nb_face_core_lerp()` (interpolação 220 ms). Também calcula, por coluna,
a geometria com AA sub-pixel do olho (`nb_face_core_eye_column()`) sem
tocar em nenhuma lib gráfica — só matemática, testável em `host_test/`.

Casca C++ (`shell/nb_face_renderer_shell.cpp/.h`) atrás de `extern "C"`:
único ponto do firmware que instancia um `LGFX_Sprite` (LovyanGFX,
vendorizado como git submodule em `vendor/LovyanGFX/`, mesma convenção do
v1 — não está no ESP Component Registry). O sprite é amarrado
(`setBuffer`) ao back buffer do `display_hal` (S2.1) via
`nb_face_renderer_shell_bind_buffer()`, sem alocar framebuffer próprio.
`nb_face_renderer_shell_draw_dirty()` desenha os dois olhos e retorna a união
conservadora entre a região anterior e a atual para o `display_hal` mandar só
o dirty rectangle (S4.1a). O primeiro desenho retorna tela cheia; depois o
normal são bounds com margem ao redor dos olhos. `nb_face_renderer_shell_draw()`
continua existindo como wrapper compatível. Além do estado paramétrico, aceita
`gaze_x`/`gaze_y`, `width_l`/`width_r` (multiplicador de largura por
olho, clamp `[0.5, 1.5]` — `CURIOUS_TILT` do `idle_engine`, S2.4) e
`tilt` (assimetria vertical entre os olhos — `HEAD_TILT_HOLD`).

`main.c` (S2.4) roda o `idle_engine` sobre a expressão `NEUTRAL`,
somando drift/blink/gaze/largura/roll ao desenho — confirmado em
bancada (gate do S2.2: paridade visual + fps ≥ 30; gate do S2.4:
catálogo de motifs de `VISUAL.md` §3).
