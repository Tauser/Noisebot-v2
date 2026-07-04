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
`nb_face_renderer_shell_draw()` desenha os dois olhos e delega o
push/swap ao `display_hal`.

`main.c` cicla as 10 expressões com hold + interpolação de 220 ms numa
task própria (`nb_app_main_face_demo_task`) — padrão de bring-up para
comparação visual lado a lado com o v1 em bancada (gate do S2.2:
paridade visual + fps ≥ 30 medido).
