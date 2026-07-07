# tools/

| Script | Função | Fase |
| --- | --- | --- |
| `enter_idf_env.ps1` | exporta o ambiente ESP-IDF v5.5.4 correto do projeto (`IDF_PATH`, `IDF_TOOLS_PATH`, `IDF_PYTHON_ENV_PATH`) e pode invocar `idf.py` sem cair no fallback antigo de `%USERPROFILE%\.espressif` | bancada / manutenção |
| `com5_bench_prep.ps1` | checklist de bancada para serial (`COM5` por default): lista portas, mostra processos candidatos a segurar monitor e, opcionalmente, encerra monitores conhecidos | bancada / manutenção |
| `scan_secrets.py` | varredura de credenciais/chaves — job `secrets-scan` do CI | ativo desde o commit 1 |
| `sram_gate.py` | extrai uso estático de SRAM do `.map` e falha acima do teto (`docs/QUALITY.md`) | S1.1 |
| `nbp2_codegen/` | gera C/Python a partir de `protocol/nbp2.yaml` + golden tests | S1.7 |
