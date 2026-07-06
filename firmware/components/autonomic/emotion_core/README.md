# emotion_core

Componente `firmware/components/autonomic/emotion_core` — S2.5, camada L4
(`ARCHITECTURE.md`). Núcleo C17 puro (`emotion_core.c/.h`): vetor 2D
(valência × ativação, `[-1, +1]`, `BEHAVIOR.md` §2), `nb_emotion_core_
apply_stimulus()` (soma deltas com clamp por eixo) e `nb_emotion_core_
tick(dt_ms)` (decaimento exponencial analítico; `tau = 60/ln(20) ≈
20.03s`, derivado de "constante ~60s até <5% do pico" — mesma constante
de S2.5, só o alvo do decay mudou, ver item 6 abaixo).
`nb_emotion_core_nearest_expression()` mapeia o vetor pra expressão-âncora
mais próxima (distância euclidiana, `VISUAL.md` §2) reaproveitando
`nb_face_expr_t` do renderer (S2.2, camada L3 adjacente) em vez de
duplicar o enum — mantida por compatibilidade, mas não é mais o caminho
principal de renderização (ver item 6).

Sem FreeRTOS/ESP-IDF/NVS. Sem casca própria ainda (mesma regra do
`event_bus`/`tiny_fsm`/`idle_engine`); `main.c` tickava a expressão-âncora
resultante interpolando em 220ms (S2.2), com o `idle_engine` (S2.4)
sobrepondo motifs por cima -- superseded pelo item 6 abaixo.

## S3.7 completo — item 6: campo contínuo + temperamento + circadiano

**Escopo revisto pelo usuário (2026-07-06):** as 6 âncoras fora dos hubs
(`CURIOUS/SLEEPY/FOCUSED/SUSPICIOUS/SURPRISED/ALARMED`) saem de uso —
não precisa mais preservar fallback nearest-neighbor pra elas (diverge do
RFC original, que previa mantê-las intocadas; decisão de produto do
usuário substitui isso).

- **`nb_emotion_core_resolve_face(state, out)`** — resolve o vetor (v,a)
  diretamente num `nb_face_state_t`, por blend contínuo entre os 4 hubs
  (`NEUTRAL/HAPPY/SAD/ANGRY`). Pesos por distância inversa ao quadrado
  (Shepard), normalizados; se o vetor coincide "exatamente" com uma
  âncora (distância² < 1e-8), retorna aquela âncora bit-a-bit em vez de
  passar pelo blend — sem divisão por zero, e satisfaz o host-test do RFC
  §9 ("campo passa exatamente pelas âncoras calibradas"). Usa
  `nb_face_core_blend()` (novo, `renderer.c` — blend N-way ponderado dos
  21 campos de `nb_face_state_t`, mesma camada L3 que já fornecia
  `nb_face_core_lerp()`).
- **Temperamento:** o alvo do decay muda de `(0,0)` pra
  `(+0.10, +0.05)` — "em paz, o Noise é sutilmente caloroso e desperto"
  (RFC §7), consequência visível no micro-sorriso do `NEUTRAL` (item 5).
  A constante de decaimento (tau) não mudou, só o ponto de repouso.
- **Offset circadiano:** `nb_emotion_core_set_circadian_offset(state,
  arousal_offset)` (novo, público) soma ao alvo de ativação — gancho
  exposto, **ainda não chamado por nenhuma casca** (default 0.0, sem
  efeito). Entrada por parâmetro em vez de `emotion_core` ler
  `circadian_core` diretamente -- mesma lição do item 2 (energia): a
  exceção de mexer em `circadian_core` prevista no plano acabou não
  sendo necessária aqui também.

`main.c`: o nearest-neighbor + transição de 220ms (S2.2/S2.5) deu lugar a
uma chamada direta de `nb_emotion_core_resolve_face()` por tick -- o
próprio vetor já se move suavemente (decay/estímulo), então o blend por
posição já entrega a face contínua sem precisar de um timer de easing
artificial. **S2.2 deixa de ser critério de paridade a partir daqui**
(já previsto no ROADMAP).

`tools/run_host_tests.py` precisou de um ajuste real: `emotion_core.c`
agora chama função de `renderer.c` de verdade (`nb_face_core_blend()`),
não só usa o header como antes -- o script passou a parsear
`REQUIRES`/`PRIV_REQUIRES` do `CMakeLists.txt` real de cada componente
(que já declarava `REQUIRES renderer`) e linkar as fontes transitivas,
em vez de assumir "só header, nunca símbolo pra linkar".

Host-test cobre: estado inicial neutro, clamp de estímulo por eixo e
acumulado, decaimento convergindo pro temperamento (não mais zero) de
ambos os lados, nearest-neighbor batendo exatamente em cada uma das 10
âncoras (mantido), `resolve_face()` batendo exatamente em cada um dos 4
hubs e variando continuamente entre eles (sem salto grande passo a
passo), `NULL` seguro.

**Bug real achado em bancada (S2.5, histórico):** o pulso sintético de
bring-up era aplicado a cada 15s, mais curto que a constante de
decaimento (~60s) — os pulsos se acumulavam mais rápido do que decaíam,
travando a expressão oscilando sem nunca voltar a `NEUTRAL`. Corrigido
aumentando o intervalo para 90s; removido de vez em S3.2 quando o toque
real chegou.

**Fora do escopo desta fatia:** persistência da última expressão em NVS,
decay assimétrico (S3.8), variantes episódicas (item 7 deste plano).
