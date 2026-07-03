# nbp2

Casca ESP-IDF fininha em volta do C gerado a partir de `protocol/nbp2.yaml`
(ver `protocol/README.md` e `docs/PROTOCOL.md`).

Este componente **não gera nada** — só registra `protocol/generated/c/nbp2.c`
e `nbp2.h` como fontes/includes. O codegen (`protocol/codegen/
generate_nbp2.py`) usa PyYAML, que deliberadamente não é dependência do
toolchain do ESP-IDF (decisão registrada em `docs/ROADMAP.md` §S1.7, para não
arriscar o job `firmware-build` do CI). Por isso os arquivos gerados
precisam existir no disco **antes** deste componente compilar:

```
pip install pyyaml   # uma vez
python protocol/codegen/generate_nbp2.py
```

Se `protocol/generated/c/nbp2.c` não existir, o CMake falha com uma mensagem
clara em vez de um erro de "arquivo não encontrado" difícil de rastrear.

No CI, o job `firmware-build` roda o codegen com o Python padrão do runner
antes de chamar a action do ESP-IDF — os dois toolchains ficam desacoplados
de propósito.

Sem `host_test/`: este componente não tem código próprio para testar (o
C gerado já é coberto por `tools/check_protocol_golden.py`).
