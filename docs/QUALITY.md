# NoiseBot 2 — Qualidade e Enforcement

A tese (P2): **qualidade que não é gate executável não existe.** O v1 tinha
excelente arquitetura em prosa e 3 arquivos de host-test para 43k LOC; este
documento é o contrato para não repetir isso.

## 1. CI (GitHub Actions) — obrigatório verde para merge

Workflow único `ci.yml`, disparado em **todo push/PR, sem filtro de paths**
(cicatriz v1: workflow filtrado por `server/**` deixou 20 testes de contrato
mortos em silêncio após um `git mv`).

| Job | O quê | Falha se |
| --- | --- | --- |
| `firmware-build` | `idf.py build` com `-Wall -Wextra -Werror` | qualquer warning |
| `host-tests` | compila e roda todos os `host_test/` (CMake host) | qualquer assert |
| `protocol-golden` | codegen do YAML → vetores canônicos C e Python → compara bytes | 1 byte divergente |
| `server-tests` | `pytest` matriz Python 3.10/3.11 | qualquer teste |
| `budget-gates` | SRAM estática do `.map` ≤ teto; imagem ≤ partição OTA | número estourado |
| `secrets-scan` | varredura de tokens/chaves/credenciais no diff | qualquer achado |
| `lint` | clang-format --dry-run + ruff | qualquer desvio |

## 2. Definition of Done (qualquer feature/subfase)

1. Código respeitando `ARCHITECTURE.md` (camadas, núcleo/casca).
2. Host-test do núcleo novo/alterado (cobrindo o caminho de falha, não só o
   feliz).
3. Se toca protocolo: entrada no `nbp2.yaml` + golden test.
4. Budgets respeitados (CI prova).
5. 5–15 linhas de doc no README do componente (contrato, task, prioridade).
6. Se é subfase do roadmap: critérios de saída registrados com evidência.

## 3. Testes de invariante (permanentes, nunca removidos)

| Invariante | Teste |
| --- | --- |
| X→IDLE limpa tudo (expr/gaze/pescoço/overlays/led) | table-driven, 100% das transições da tiny_fsm |
| Pool de eventos não dropa evento não-safety sob burst alvo | teste de carga com perfil registrado (touch+áudio+render simultâneos) |
| Evento de safety nunca bloqueado por backpressure | teste de fila cheia + publish de safety |
| ACK ≠ commit; timeout é ambíguo | testes do núcleo NBP/2 com transporte simulado |
| Reboot (`boot_id` novo) invalida pendências | idem |
| Comando mutador com mesmo `seq` é idempotente | idem |
| motion_safety veta posição fora de limite em qualquer estado ≠ ARMED | host-test do núcleo de safety |

## 4. Budgets mensuráveis (metas de produto)

| Métrica | Alvo | Instrumento |
| --- | --- | --- |
| Touch → reação visível | < 80 ms p95 | timestamps no bus, exposto em `STATUS` |
| Wake → overlay listening | < 250 ms p95 | idem |
| Fim de fala → primeiro áudio TTS | < 1,5 s p95 (LLM local) | métricas TurnEngine |
| Render | 30 fps sustentado, drops < 0,1% | contador no face_service |
| Snapshot JPEG → server | < 400 ms p95 | transfer metrics |
| Boot → IDLE vivo | < 3 s | log de fases do boot_manager |
| Soak | 7 dias sem crash/leak (heap estável) | contador NVS de reset + heap trace |

Números só mudam com medição registrada (P5).

## 5. Observabilidade

- Logs estruturados (nível, módulo, ts monotônico) → ring em RAM + flush
  assíncrono para SD; espelhados ao server quando conectado.
- Ring de eventos do bus com dump em shutdown **e em panic** (coredump
  partition + `esp_core_dump`).
- `STATUS` periódico ao server: heap, PSRAM livre, fps, drops, RSSI, estado
  FSM, emotion — o server persiste e o dashboard plota. Observabilidade é
  feature de S1, não polish de S7 (cicatriz v1: telemetria rica presa no
  serial).

## 6. Smoke de bancada por release

Checklist manual assinado (enquanto não houver HIL): boot < 3 s, face viva,
touch responde, wake→conversa, snapshot, timer dispara, OTA ida-e-volta,
queda de server com degradação suave. Registrado em `docs/releases/`.

## 7. Convenções

- clang-format versionado na raiz do firmware; ruff no server. Estilo é
  máquina, não opinião de review.
- Commits pequenos, um ID de roadmap por commit (`S2.3: ...`).
- Proibido versionar builds, logs de bancada e binários de teste (lição H10);
  `scratch/` e `logs/` existem para isso e são gitignored.
