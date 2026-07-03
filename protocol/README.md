# NBP/2 — schema e codegen

Fonte de verdade única do protocolo robô↔mente: **`nbp2.yaml`**.

Regras (P4, detalhes em `docs/PROTOCOL.md`):

- O codegen (S1.7, em `codegen/generate_nbp2.py`) lê `nbp2.yaml` com PyYAML
  (única dependência externa do toolchain; `pip install pyyaml` no CI) e gera:
  - `generated/c/nbp2.h/.c` — IDs de mensagem, framing, CRC32, comparação
    timing-safe de token, structs por mensagem e encode/decode CBOR em C17,
    zero malloc;
  - `generated/python/nbp2.py` — os mesmos IDs, dataclasses por mensagem e
    encode/decode CBOR equivalentes.
- CBOR é implementado à mão nos dois lados (array/uint/int/bytes/text/
  float32, forma canônica curta do RFC 8949) pelo próprio gerador — não usa
  `cbor2` nem nenhuma lib CBOR de terceiros, justamente para garantir que os
  bytes de C e Python sejam idênticos por construção, não por sorte de duas
  implementações de bibliotecas diferentes concordarem.
- `generated/` é saída de build (gitignored) — regenerado no CI e localmente.
- **Proibido:** editar código gerado; declarar constante de protocolo fora do
  YAML; espelhar structs à mão em qualquer linguagem.
- Golden test (`tools/check_protocol_golden.py`): compila o C gerado no host,
  roda casos com valores fixos e compara bytes reais contra o Python gerado —
  nos dois sentidos (C codifica/Python decodifica e vice-versa). 1 byte
  divergente = vermelho.
- Mudança de wire format exige bump de versão no YAML (o CI trava sem bump).

Status: `nbp2.yaml` está em **v0 draft** — S1.7 já protege o envelope de
wire (SOF/len/type/seq/payload/CRC32) e os payloads CBOR de todas as 26
mensagens com golden test C↔Python nos dois sentidos. Pendente: persistência/
leitura do token em NVS, transporte TCP com reconexão/backoff, soak de 100
reconexões.
