# NoiseBot 2 — Memória: Curto, Médio e Longo Prazo

Especificação das camadas de memória, quem é dono de cada uma, e as políticas
de retenção/privacidade. Regra de ouro (P1): memória que precisa de
significado mora na mente; memória que precisa sobreviver offline mora no
corpo.

## 1. Visão geral das camadas

| Camada | Horizonte | Dono | Meio | Sobrevive a |
| --- | --- | --- | --- | --- |
| Trabalho (curto) | segundos–minutos | corpo + mente | RAM | nada |
| Sessão/episódica (médio) | horas–semanas | mente | SQLite | restart do server |
| Identidade/semântica (longo) | permanente | mente, com espelho no corpo | SQLite + NVS | tudo (até wipe) |
| Diário operacional | dias–meses | corpo (escrita) | SD + server | análise/debug |

## 2. Curto prazo (trabalho)

**No corpo (RAM, nunca persiste):**
- vetor emocional (decai a neutral em ~60 s — `BEHAVIOR.md` §2);
- estado da FSM, overlays ativos, sessão de voz corrente;
- ring de auditoria do event bus (64 eventos, dump em shutdown/panic).

**Na mente (RAM):**
- contexto do turno: transcrição parcial, janela de mensagens da LLM;
- cache de anexos do dashboard (herda política v1: em memória, ~30 min,
  nunca disco);
- resultados recentes de tools/busca (TTL curto).

Perda aceitável por design: reboot zera tudo desta camada sem dano — o robô
acorda neutro e a conversa reabre pela camada seguinte.

## 3. Médio prazo (episódica) — SQLite na mente

Fonte de verdade: banco SQLite do server (herda o desenho C0 do v1:
migrations versionadas, idempotência, paginação).

- **Conversas**: turnos completos (quem, quando, texto, idioma, métricas),
  organizados por conversa; "vamos continuar" recupera a conversa certa sem
  misturar contextos.
- **Eventos episódicos**: interações notáveis (primeiro toque do dia, timer
  disparado, usuário reconhecido — quando visão voltar), com timestamp do
  corpo convertido via TIME_SYNC.
- **Métricas**: latências, contadores de sessão (para o dashboard, não para
  a LLM).

Retenção: conversas mantidas até apagamento pelo usuário; eventos episódicos
com janela deslizante (default 90 dias, configurável); métricas agregadas
após 30 dias.

## 4. Longo prazo (identidade e semântica)

**Na mente (SQLite, autoridade):**
- **Perfil do usuário**: nome, relação, idioma, apelido do robô, modo de
  persona, estilo de interação (declarado, não biométrico);
- **Fatos e preferências**: `remember_fact`/`forget_fact`/`recall` (tools da
  LLM, herdadas do v1) — cada fato com origem, data e contador de uso;
- **Sumários de relacionamento**: o que a persona sabe sobre a convivência
  (gerados periodicamente a partir da camada episódica; a episódica pode
  expirar, o sumário fica).

**No corpo (NVS, espelho mínimo offline-first):**
- snapshot do perfil (`nb_persona`): o robô sabe *com quem vive* mesmo sem
  server;
- última expressão discreta (âncora de boot, clampeada a não-negativa);
- config, calibrações, flags de safety, timers/alarmes pendentes
  (`schedule_core`) — disparam offline;
- contadores de vida: boots, tempo total, resets por watchdog.

**Sincronização:** a mente é autoridade do perfil; ao alterar, empurra o
snapshot ao corpo (mensagem NBP/2 dedicada, idempotente por versão do
perfil). O corpo nunca inventa memória: sem server, responde com o snapshot
e fatos não são consultáveis (a voz nem opera offline — sem contradição).

## 5. Diário operacional

- **Corpo → SD**: logs estruturados (ring RAM → worker assíncrono), crash
  dumps (coredump partition), contadores de drop. Rotação por tamanho;
  nunca bloqueia tasks ≥ 10 (regra herdada).
- **Corpo → mente**: espelho dos logs quando conectado (`STATUS` + stream de
  log), persistido pelo server para análise no dashboard.
- Logs nunca contêm segredos (gate `secrets-scan` + regra de logger S1.3) e
  nunca contêm áudio/transcrições — telemetria é operacional, não conteúdo.

## 6. Privacidade e controle (promessa de produto)

- Nada desta página sai da LAN. Sem nuvem, sem conta.
- **Wipe por camadas** no dashboard: (a) conversas; (b) fatos/preferências;
  (c) perfil completo + snapshot NVS (robô volta a "recém-adotado");
  (d) tudo. Wipe do perfil também limpa o espelho no corpo (comando NBP/2
  com confirmação).
- Transcrições de voz são conversas (camada 3) — apagáveis; áudio cru nunca
  é persistido em disco em nenhum dos lados.
- Anexos do dashboard: só memória, expiram sozinhos (herda política v1).

## 7. Fases de entrada (referência ao ROADMAP)

| Peça | Fase |
| --- | --- |
| NVS: config, âncora de expressão, contadores | S1.4 |
| Timers persistentes (schedule_core) | S3.5 |
| SD: logs operacionais | S1.3/S0.3 |
| SQLite: conversas (episódica) | S4.4 |
| Perfil + snapshot NVS + sync | S7.2 |
| Fatos/preferências (tools) + wipe por camadas | S7.2/S7.4 |
| Sumários de relacionamento | pós-v2.0 (exige episódica estável) |
