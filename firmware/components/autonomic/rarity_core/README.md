# rarity_core

Componente `firmware/components/autonomic/rarity_core` — S3.8, item 8,
camada L4 (`RFC-VIDA-V2.md` §10). Núcleo C17 puro das raridades: `SNEEZE`
(~1/dia), `DREAM` (<=1/noite, em `SLEEPING`), `STARGAZE` (<=1/noite,
`NIGHT`). Único item do plano S3.8 que toca protocolo — os 3 contadores
mapeiam direto pros campos novos de `STATUS`
(`rarity_sneeze_count`/`rarity_dream_count`/`rarity_stargaze_count`,
`protocol/nbp2.yaml`, bump 0.1->0.2).

Só arbitra taxa e contador -- **não decide quando tentar disparar** cada
raridade (RFC não especifica gatilho narrativo, só o teto). `DREAM`/
`STARGAZE` são "por sessão" (elegíveis de novo quando a condição de estado
transiciona de fora pra dentro), não por relógio de 24h -- o clock do
`circadian_core` roda acelerado em bancada (S3.4), então "uma noite" é
delimitado pela própria fase `NIGHT`/`SLEEPING`, não por tempo de parede.
`SNEEZE` não depende de estado -- intervalo mínimo desde o último disparo.

Sem shell ainda -- entra quando ligado a `circadian_core`
(`NIGHT`)/`tiny_fsm` (`SLEEPING`) e ao `mind_link_shell` (popular os 3
campos de `STATUS` a cada envio).
