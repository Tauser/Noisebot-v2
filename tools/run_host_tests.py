"""Compile and run firmware host tests.

The tests are plain C17 binaries on purpose: firmware cores must stay free of
ESP-IDF and FreeRTOS dependencies until their shell layer.
"""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
FIRMWARE = ROOT / "firmware"
BUILD = ROOT / "scratch" / "host_tests"


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

    tests = sorted(FIRMWARE.glob("components/**/host_test/test_*.c"))
    if not tests:
        print("host-tests: nenhum teste encontrado")
        return 1

    for test in tests:
        component_dir = test.parent.parent
        exe = BUILD / f"{component_dir.name}_{test.stem}"
        if os.name == "nt":
            exe = exe.with_suffix(".exe")

        cmd = [
            cc,
            "-std=c17",
            "-Wall",
            "-Wextra",
            "-Werror",
            f"-I{component_dir}",
            str(component_dir / f"{component_dir.name}.c"),
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
