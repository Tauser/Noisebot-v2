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

## S3.7 completo — item 5: boca

`nb_face_state_t` ganhou dois campos (`mouth_open` [0,1], `mouth_curve`
[-1,+1] — RFC-VIDA-V2.md §3), interpolados por `nb_face_core_lerp()` como
os demais 18 campos, e uma nova geometria por coluna
(`nb_face_core_mouth_column()`): mais simples
que o olho (sem squint/canto/arredondamento), a faixa toda curva nas
pontas via parábola (mesma técnica de `curve_top`/`curve_bottom`) e
**nunca desaparece** mesmo com `mouth_open=0` — a curvatura (micro-sorriso
do `NEUTRAL`, franzido de `SAD`/`ANGRY`) precisa aparecer com a boca
fechada.

Só os 4 hubs (`NEUTRAL/HAPPY/SAD/ANGRY`) ganharam boca não-neutra na
tabela de âncoras; as outras 6 continuam "boca neutra" (`0,0`),
intocadas — RFC decidiu isso explicitamente pra esta fase. Estático: sem
campo contínuo/variantes ainda (item 6), sem visemas de fala (S4 voz).

Casca (`nb_face_renderer_shell.cpp`): posição fixa (`kMouthCenterX`,
`kMouthBaseY`), não segue gaze/tilt/x_off nesta fase. `draw_mouth()`
mesmo padrão de `draw_eye()` (banda coluna-a-coluna com AA). O retângulo
de flush (`face_dirty_rect`) agora une olho esquerdo + direito + boca —
`mouth_dirty_rect()` com padding cobrindo o desvio máximo de curvatura
(14px) + meia-altura máxima (20px). Zero mudança em `main.c`: a boca já
chega interpolada dentro de `current` (o `nb_face_state_t` que o loop já
passa pro desenho), sem precisar de parâmetro novo em
`draw()`/`draw_dirty()`.

Host-tests: só os 4 hubs têm boca não-neutra (as outras 6 seguem
`0,0`); coluna nunca fica vazia mesmo fechada; sorriso curva as pontas
pra cima em relação ao centro; lerp interpola os 2 campos novos. Sem
host-test de "parece bom" — isso é bancada (boca estática nos 4 hubs).
