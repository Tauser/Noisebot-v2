# NBP/2 — schema e codegen

Fonte de verdade única do protocolo robô↔mente: **`nbp2.yaml`**.

Regras (P4, detalhes em `docs/PROTOCOL.md`):

- O codegen (S1.7, em `codegen/`) gera:
  - `generated/c/nbp2.h/.c` — IDs de mensagem, framing, CRC32 e comparação
    timing-safe de token em C17, tabelas estáticas, zero malloc;
  - `generated/python/nbp2.py` — os mesmos IDs e helpers de framing.
- `generated/` é saída de build (gitignored) — regenerado no CI e localmente.
- **Proibido:** editar código gerado; declarar constante de protocolo fora do
  YAML; espelhar structs à mão em qualquer linguagem.
- Golden tests (`golden/`): vetores canônicos por mensagem; o CI codifica em C
  e em Python e compara bytes. 1 byte divergente = vermelho.
- Mudança de wire format exige bump de versão no YAML (o CI trava sem bump).

Status: `nbp2.yaml` está em **v0 draft** — S1.7 já protege o envelope de
wire (SOF/len/type/seq/payload/CRC32) com golden test C↔Python. Payloads
CBOR/structs por mensagem entram na próxima fatia da mesma subfase.
