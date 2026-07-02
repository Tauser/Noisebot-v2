# NBP/2 — schema e codegen

Fonte de verdade única do protocolo robô↔mente: **`nbp2.yaml`**.

Regras (P4, detalhes em `docs/PROTOCOL.md`):

- O codegen (construído em S1.7, em `codegen/`) gera:
  - `generated/c/nbp2_msgs.h/.c` — encoder/decoder C17, tabelas estáticas,
    zero malloc;
  - `generated/python/nbp2_msgs.py` — dataclasses + encode/decode.
- `generated/` é saída de build (gitignored) — regenerado no CI e localmente.
- **Proibido:** editar código gerado; declarar constante de protocolo fora do
  YAML; espelhar structs à mão em qualquer linguagem.
- Golden tests (`golden/`): vetores canônicos por mensagem; o CI codifica em C
  e em Python e compara bytes. 1 byte divergente = vermelho.
- Mudança de wire format exige bump de versão no YAML (o CI trava sem bump).

Status: `nbp2.yaml` está em **v0 draft** — congela na S1.7.
