"""Compile and run firmware host tests.

The tests are plain C17 binaries on purpose: firmware cores must stay free of
ESP-IDF and FreeRTOS dependencies until their shell layer.
"""

from __future__ import annotations

import os
import re
import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
FIRMWARE = ROOT / "firmware"
BUILD = ROOT / "scratch" / "host_tests"

_REQUIRES_RE = re.compile(r"\b(?:PRIV_)?REQUIRES\b([^)]*)")


def parse_requires(cmakelists: Path) -> list[str]:
    """Nomes de componente em REQUIRES/PRIV_REQUIRES de um CMakeLists.txt real
    -- mesma fonte de verdade que o build de verdade (idf_component_register),
    em vez de uma lista mantida à mão aqui."""
    text = cmakelists.read_text(encoding="utf-8")
    names: list[str] = []
    for match in _REQUIRES_RE.finditer(text):
        names.extend(match.group(1).split())
    return names


def collect_sources_with_requires(component_dir: Path, dir_by_name: dict[str, Path]) -> list[Path]:
    """Fontes .c do componente +, transitivamente, dos componentes que ele
    declara em REQUIRES/PRIV_REQUIRES -- necessário desde que emotion_core
    (S3.7 completo, item 6) passou a chamar função de renderer.c de verdade,
    não só usar o header (a suposição antiga de "só headers, sem símbolo pra
    linkar" parou de valer pra esse par)."""
    visited: set[Path] = set()
    queue = [component_dir]
    sources: list[Path] = []

    while queue:
        current = queue.pop()
        if current in visited:
            continue
        visited.add(current)
        sources.extend(sorted(current.glob("*.c")))

        cmakelists = current / "CMakeLists.txt"
        if cmakelists.exists():
            for name in parse_requires(cmakelists):
                dep_dir = dir_by_name.get(name)
                if dep_dir is not None and dep_dir not in visited:
                    queue.append(dep_dir)

    return sources


def compiler() -> str:
    configured = os.environ.get("CC")
    if configured:
        return configured
    for candidate in ("gcc", "cc", "clang"):
        found = shutil.which(candidate)
        if found:
            return found
    raise SystemExit("host-tests: nenhum compilador C encontrado (gcc/cc/clang)")


def main() -> int:
    cc = compiler()
    BUILD.mkdir(parents=True, exist_ok=True)

    # Flags extras (ex.: NB_HOST_TEST_CFLAGS="-DNB_IDLE_V2_SPIKE=0") -- usado
    # pra rodar a mesma suíte em configs alternativas de flag de compilação
    # (S3.7 spike: idle_engine tem comportamento sob NB_IDLE_V2_SPIKE, ligado
    # por padrão no header; passar =0 aqui roda a suíte inteira com o motif
    # antigo, sem precisar de um segundo script).
    extra_cflags = os.environ.get("NB_HOST_TEST_CFLAGS", "").split()

    tests = sorted(FIRMWARE.glob("components/**/host_test/test_*.c"))
    if not tests:
        print("host-tests: nenhum teste encontrado")
        return 1

    # Núcleos puros podem incluir o header de outro núcleo puro adjacente
    # (ex.: emotion_core usa o enum de expressões do renderer, ARCHITECTURE.md
    # §2 -- L4 chamando L3). Todos os diretórios de núcleo entram no include
    # path de todo mundo -- majoritariamente só headers, mas ver
    # collect_sources_with_requires() pro caso de dependência real de símbolo.
    component_dirs = sorted({test.parent.parent for test in tests})

    # Mapa nome-de-componente -> diretório, pra resolver REQUIRES/
    # PRIV_REQUIRES de qualquer CMakeLists.txt real do firmware (não só os
    # que têm host_test).
    dir_by_name = {p.parent.name: p.parent for p in FIRMWARE.glob("components/**/CMakeLists.txt")}

    for test in tests:
        component_dir = test.parent.parent
        exe = BUILD / f"{component_dir.name}_{test.stem}"
        if os.name == "nt":
            exe = exe.with_suffix(".exe")

        # Um componente pode ter mais de um arquivo .c de núcleo (ex.:
        # idle_engine ganhou nb_breath.c/nb_attention.c em S3.7 além de
        # idle_engine.c), e pode depender de símbolos de outro componente
        # (ex.: emotion_core chama nb_face_core_blend() de renderer.c, S3.7
        # completo item 6) -- compila todos os fontes do diretório mais os
        # de qualquer REQUIRES transitivo, não só <componente>.c.
        core_sources = collect_sources_with_requires(component_dir, dir_by_name)

        cmd = [
            cc,
            "-std=c17",
            "-Wall",
            "-Wextra",
            "-Werror",
            *extra_cflags,
            *[f"-I{d}" for d in component_dirs],
            *[str(src) for src in core_sources],
            str(test),
            "-o",
            str(exe),
            "-lm",
        ]
        print("host-tests: compile", test.relative_to(ROOT))
        subprocess.run(cmd, cwd=ROOT, check=True)
        print("host-tests: run", exe.relative_to(ROOT))
        subprocess.run([str(exe)], cwd=ROOT, check=True)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
