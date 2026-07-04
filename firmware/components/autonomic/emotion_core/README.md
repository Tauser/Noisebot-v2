# emotion_core

Componente `firmware/components/autonomic/emotion_core` — S2.5, camada L4
(`ARCHITECTURE.md`). Núcleo C17 puro (`emotion_core.c/.h`): vetor 2D
(valência × ativação, `[-1, +1]`, `BEHAVIOR.md` §2), `nb_emotion_core_
apply_stimulus()` (soma deltas com clamp por eixo) e `nb_emotion_core_
tick(dt_ms)` (decaimento exponencial analítico rumo a (0,0); `tau =
60/ln(20) ≈ 20.03s`, derivado de "constante ~60s até <5% do pico").
`nb_emotion_core_nearest_expression()` mapeia o vetor pra expressão-âncora
mais próxima (distância euclidiana, `VISUAL.md` §2) reaproveitando
`nb_face_expr_t` do renderer (S2.2, camada L3 adjacente) em vez de
duplicar o enum — `tools/run_host_tests.py` ganhou include path cruzado
entre núcleos puros pra viabilizar isso sem linkar `renderer.c`.

Sem FreeRTOS/ESP-IDF/NVS. Sem casca própria ainda (mesma regra do
`event_bus`/`tiny_fsm`/`idle_engine`); `main.c` aplica um pulso de
estímulo sintético periódico (substituto do toque real, que só chega em
S3.1) e interpola a expressão-âncora resultante em 220ms, com o
`idle_engine` (S2.4) sobrepondo motifs por cima.

Host-test cobre: estado inicial neutro, clamp de estímulo por eixo e
acumulado, decaimento chegando a <5% em 60s e monotônico dos dois lados,
nearest-neighbor batendo exatamente em cada uma das 10 âncoras, `NULL`
seguro.

**Bug real achado em bancada:** o pulso sintético de bring-up era
aplicado a cada 15s, mais curto que a constante de decaimento (~60s) —
os pulsos se acumulavam mais rápido do que decaíam, travando a expressão
oscilando sem nunca voltar a `NEUTRAL`. Corrigido aumentando o intervalo
para 90s (`main.c`).

**Fora do escopo desta fatia:** persistência da última expressão em NVS
e a tabela de reflexos locais (touch/voz reais) ficam para
`reflex_engine` (S3.2), quando o `touch_service` existir.
