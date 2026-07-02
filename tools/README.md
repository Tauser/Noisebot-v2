# tools/

| Script | Função | Fase |
| --- | --- | --- |
| `scan_secrets.py` | varredura de credenciais/chaves — job `secrets-scan` do CI | ativo desde o commit 1 |
| `sram_gate.py` | extrai uso estático de SRAM do `.map` e falha acima do teto (`docs/QUALITY.md`) | S1.1 |
| `nbp2_codegen/` | gera C/Python a partir de `protocol/nbp2.yaml` + golden tests | S1.7 |
