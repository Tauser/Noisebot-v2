#!/usr/bin/env python3
"""Generate NBP/2 artifacts and compare C/Python golden wire bytes."""

from __future__ import annotations

import importlib.util
import os
import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "scratch" / "protocol_golden"
GENERATED = ROOT / "protocol" / "generated"


def compiler() -> str:
    configured = os.environ.get("CC")
    if configured:
        return configured
    for candidate in ("gcc", "cc", "clang"):
        found = shutil.which(candidate)
        if found:
            return found
    raise SystemExit("protocol-golden: nenhum compilador C encontrado (gcc/cc/clang)")


def load_generated_python():
    module_path = GENERATED / "python" / "nbp2.py"
    spec = importlib.util.spec_from_file_location("generated_nbp2", module_path)
    if spec is None or spec.loader is None:
        raise SystemExit("protocol-golden: falha ao carregar nbp2.py gerado")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def write_c_golden_source(path: Path) -> None:
    path.write_text(
        r'''#include "nbp2.h"

#include <stdio.h>
#include <stdint.h>

static void print_hex(const uint8_t *data, size_t len)
{
    size_t i;

    for (i = 0u; i < len; ++i) {
        printf("%02x", data[i]);
    }
}

int main(void)
{
    const uint8_t hello_payload[] = {0xA1u, 0x01u, 0x02u};
    const uint8_t token_a[] = {1u, 2u, 3u, 4u};
    const uint8_t token_b[] = {1u, 2u, 3u, 4u};
    const uint8_t token_c[] = {1u, 2u, 3u, 5u};
    uint8_t frame[64];
    nbp2_frame_view_t view;
    size_t frame_len = 0u;

    if (nbp2_encode_frame(NBP2_MSG_HELLO, 0x01020304u, hello_payload,
                          sizeof(hello_payload), frame, sizeof(frame),
                          &frame_len) != NBP2_OK) {
        return 1;
    }
    if (nbp2_decode_frame(frame, frame_len, &view) != NBP2_OK) {
        return 2;
    }
    if (view.type != NBP2_MSG_HELLO || view.seq != 0x01020304u ||
        view.payload_len != sizeof(hello_payload)) {
        return 3;
    }

    printf("hello=");
    print_hex(frame, frame_len);
    printf("\n");

    if (nbp2_encode_frame(NBP2_MSG_HEARTBEAT, 7u, NULL, 0u, frame,
                          sizeof(frame), &frame_len) != NBP2_OK) {
        return 4;
    }
    printf("heartbeat=");
    print_hex(frame, frame_len);
    printf("\n");

    printf("token_equal=%d\n", nbp2_timing_safe_equal(token_a, sizeof(token_a),
                                                      token_b, sizeof(token_b)) ? 1 : 0);
    printf("token_diff=%d\n", nbp2_timing_safe_equal(token_a, sizeof(token_a),
                                                     token_c, sizeof(token_c)) ? 1 : 0);
    printf("token_len=%d\n", nbp2_timing_safe_equal(token_a, sizeof(token_a),
                                                    token_b, 3u) ? 1 : 0);
    return 0;
}
''',
        encoding="utf-8",
    )


def parse_output(output: str) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in output.splitlines():
        key, value = line.split("=", 1)
        values[key] = value
    return values


def main() -> int:
    BUILD.mkdir(parents=True, exist_ok=True)
    subprocess.run(
        [
            sys.executable,
            str(ROOT / "protocol" / "codegen" / "generate_nbp2.py"),
            "--out",
            str(GENERATED),
        ],
        cwd=ROOT,
        check=True,
    )

    nbp2 = load_generated_python()
    c_source = BUILD / "nbp2_golden.c"
    exe = BUILD / ("nbp2_golden.exe" if os.name == "nt" else "nbp2_golden")
    write_c_golden_source(c_source)

    subprocess.run(
        [
            compiler(),
            "-std=c17",
            "-Wall",
            "-Wextra",
            "-Werror",
            f"-I{GENERATED / 'c'}",
            str(GENERATED / "c" / "nbp2.c"),
            str(c_source),
            "-o",
            str(exe),
        ],
        cwd=ROOT,
        check=True,
    )
    c_output = subprocess.check_output([str(exe)], cwd=ROOT, text=True)
    c_values = parse_output(c_output)

    hello_payload = bytes([0xA1, 0x01, 0x02])
    expected = {
        "hello": nbp2.encode_frame(nbp2.MSG_HELLO, 0x01020304, hello_payload).hex(),
        "heartbeat": nbp2.encode_frame(nbp2.MSG_HEARTBEAT, 7, b"").hex(),
        "token_equal": "1" if nbp2.timing_safe_equal(b"\x01\x02\x03\x04", b"\x01\x02\x03\x04") else "0",
        "token_diff": "1" if nbp2.timing_safe_equal(b"\x01\x02\x03\x04", b"\x01\x02\x03\x05") else "0",
        "token_len": "1" if nbp2.timing_safe_equal(b"\x01\x02\x03\x04", b"\x01\x02\x03") else "0",
    }

    if c_values != expected:
        print("protocol-golden: divergencia C/Python")
        print("C:", c_values)
        print("PY:", expected)
        return 1

    print("protocol-golden: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
