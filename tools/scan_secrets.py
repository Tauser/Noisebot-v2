#!/usr/bin/env python3
"""Varredura simples de segredos no repositório (job secrets-scan do CI).

Bloqueia os vazamentos que morderam o v1: credenciais WiFi em header, tokens
em código/log, chaves de API. Não é DLP perfeito — é a rede de segurança
mínima que roda em todo push. Falsos positivos: adicionar à ALLOWLIST com
justificativa em comentário.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

# (label, pattern, sufixos isentos — docs .md podem mencionar o anti-padrão)
PATTERNS = [
    ("wifi_creds header",
     re.compile(r"wifi_creds\.h"), {".md"}),
    ("password/senha literal",
     re.compile(r"(password|senha|passwd)\s*[=:]\s*[\"'][^\"']{4,}[\"']", re.I), set()),
    ("api key literal",
     re.compile(r"(api[_-]?key|secret[_-]?key)\s*[=:]\s*[\"'][A-Za-z0-9_\-]{16,}[\"']", re.I), set()),
    ("token hex longo literal",
     re.compile(r"[\"'][0-9a-f]{32,}[\"']"), set()),
    ("private key",
     re.compile(r"-----BEGIN (RSA |EC )?PRIVATE KEY-----"), set()),
    ("openai key",
     re.compile(r"sk-[A-Za-z0-9]{20,}"), set()),
]

SCAN_EXT = {".c", ".h", ".cpp", ".hpp", ".py", ".yaml", ".yml", ".json",
            ".md", ".txt", ".ini", ".toml", ".ts", ".tsx", ".js"}
SKIP_DIRS = {".git", "build", "managed_components", "node_modules",
             "__pycache__", ".venv", "generated", "scratch"}

ALLOWLIST = set()  # ex.: {"docs/EXEMPLO.md:42"} com justificativa em comentário


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    findings = []

    for path in root.rglob("*"):
        if not path.is_file() or path.suffix not in SCAN_EXT:
            continue
        if any(part in SKIP_DIRS for part in path.parts):
            continue
        if path.name == "scan_secrets.py":
            continue
        try:
            text = path.read_text(encoding="utf-8", errors="ignore")
        except OSError:
            continue
        for lineno, line in enumerate(text.splitlines(), 1):
            for label, pattern, exempt in PATTERNS:
                if path.suffix in exempt:
                    continue
                if pattern.search(line):
                    key = "{}:{}".format(path.relative_to(root), lineno)
                    if key not in ALLOWLIST:
                        findings.append("{}: [{}] {}".format(key, label, line.strip()[:80]))

    if findings:
        print("SEGREDOS SUSPEITOS ENCONTRADOS:")
        print("\n".join(findings))
        return 1
    print("secrets-scan: limpo")
    return 0


if __name__ == "__main__":
    sys.exit(main())
