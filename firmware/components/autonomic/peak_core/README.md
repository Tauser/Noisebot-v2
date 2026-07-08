# peak_core

Componente `firmware/components/autonomic/peak_core` — S3.8, item 7, camada
L4 (`RFC-VIDA-V2.md` §4). Núcleo C17 puro da régua anti-carnaval de picos:
glifos (`HEART`/`TEARS`/`LAUGH` -- olhos-pictograma, substituem o olho) e
adornos (`STARS`/`BLUSH`/`SWEAT_DROP`/`QUESTION`/`EXCLAMATION`/`ZZZ` --
ficam junto da face). `@@` espiral (tontura) fica fora de escopo, bloqueado
por IMU (RFC §8).

Regras invioláveis: máximo 1 mecanismo por vez (slot único, mesmo padrão
de `active_motif` do `idle_engine`); 1-2s com fade (exceto `ZZZ`, que
persiste enquanto pedido, sem fade/timeout); blink pausa só durante glifo
de olho, não durante adorno; `nb_peak_core_reset_transient()` limpa em
entrada de IDLE (H7).

Não desenha nada nem conhece `emotion_core`/`arc_core`/`touch_service` --
só recebe `requested` a cada tick (decidido pela casca: pico do vetor
`|v,a|>0.7`, ou beat de um arco como o `♥` de `RECONCILE`) e arbitra qual
mecanismo está ativo. Desenho real usa o pipeline SVG->PBM já existente
(`VISUAL.md` §5), fora de escopo deste núcleo.

Sem shell ainda -- entra quando ligado a `emotion_core` (detecção de pico)
e aos arcos (`reconcile`, futuros `GRUMPY_FORGIVE`/`SEARCH` com glifo
próprio, se algum vier a ter).
