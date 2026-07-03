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


def c_byte_array(data: bytes) -> str:
    return ", ".join(f"0x{b:02x}u" for b in data)


def write_c_golden_source(path: Path, py_encoded: dict[str, bytes]) -> None:
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
    uint8_t buf[128];
    size_t len = 0u;

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

    printf("hello_frame=");
    print_hex(frame, frame_len);
    printf("\n");

    if (nbp2_encode_frame(NBP2_MSG_HEARTBEAT, 7u, NULL, 0u, frame,
                          sizeof(frame), &frame_len) != NBP2_OK) {
        return 4;
    }
    printf("heartbeat_frame=");
    print_hex(frame, frame_len);
    printf("\n");

    printf("token_equal=%d\n", nbp2_timing_safe_equal(token_a, sizeof(token_a),
                                                      token_b, sizeof(token_b)) ? 1 : 0);
    printf("token_diff=%d\n", nbp2_timing_safe_equal(token_a, sizeof(token_a),
                                                     token_c, sizeof(token_c)) ? 1 : 0);
    printf("token_len=%d\n", nbp2_timing_safe_equal(token_a, sizeof(token_a),
                                                    token_b, 3u) ? 1 : 0);

    /* Payloads CBOR: C codifica com os mesmos valores que o Python usa,
     * bytes tem que bater exatamente (RFC 8949 forma canonica curta). */
    {
        nbp2_msg_hello_t hello = {0};
        hello.proto_major = 0u;
        hello.proto_minor = 1u;
        hello.boot_id = 0x11223344u;
        hello.caps = 7u;
        hello.token[0] = 'a'; hello.token[1] = 'b'; hello.token[2] = 'c'; hello.token[3] = 'd';
        hello.token_len = 4u;
        if (nbp2_encode_hello(&hello, buf, sizeof(buf), &len) != NBP2_OK) { return 10; }
        printf("hello_cbor=");
        print_hex(buf, len);
        printf("\n");
    }
    {
        nbp2_msg_heartbeat_t heartbeat = {0};
        heartbeat.counter = 42u;
        heartbeat.uptime_ms = 123456789012ull;
        if (nbp2_encode_heartbeat(&heartbeat, buf, sizeof(buf), &len) != NBP2_OK) { return 11; }
        printf("heartbeat_cbor=");
        print_hex(buf, len);
        printf("\n");
    }
    {
        nbp2_msg_status_t status = {0};
        status.state = NBP2_FSM_STATE_IDLE;
        status.valence = 0.5f;
        status.arousal = -0.25f;
        status.heap_free = 1000u;
        status.psram_free = 2000u;
        status.fps_x10 = 300u;
        status.bus_dropped = 0u;
        status.rssi = -42;
        if (nbp2_encode_status(&status, buf, sizeof(buf), &len) != NBP2_OK) { return 12; }
        printf("status_cbor=");
        print_hex(buf, len);
        printf("\n");
    }
    {
        nbp2_msg_timer_set_t timer_set = {0};
        timer_set.timer_id = 1u;
        timer_set.fire_at_unix_ms = 1700000000000ull;
        snprintf(timer_set.label, sizeof(timer_set.label), "%s", "lembrete");
        if (nbp2_encode_timer_set(&timer_set, buf, sizeof(buf), &len) != NBP2_OK) { return 13; }
        printf("timer_set_cbor=");
        print_hex(buf, len);
        printf("\n");
    }
    {
        nbp2_msg_event_state_t event_state = {0};
        event_state.from = NBP2_FSM_STATE_IDLE;
        event_state.to = NBP2_FSM_STATE_SLEEPING;
        if (nbp2_encode_event_state(&event_state, buf, sizeof(buf), &len) != NBP2_OK) { return 14; }
        printf("event_state_cbor=");
        print_hex(buf, len);
        printf("\n");
    }

    /* Decodifica bytes que o Python gerou, prova a outra direcao. */
    {
        static const uint8_t py_hello[] = {'''
        + c_byte_array(py_encoded["hello"])
        + r'''};
        nbp2_msg_hello_t out = {0};
        if (nbp2_decode_hello(py_hello, sizeof(py_hello), &out) != NBP2_OK) { return 20; }
        printf("hello_from_py=%u,%u,%u,%u,%u:", (unsigned)out.proto_major, (unsigned)out.proto_minor,
               (unsigned)out.boot_id, (unsigned)out.caps, (unsigned)out.token_len);
        print_hex(out.token, out.token_len);
        printf("\n");
    }
    {
        static const uint8_t py_status[] = {'''
        + c_byte_array(py_encoded["status"])
        + r'''};
        nbp2_msg_status_t out = {0};
        if (nbp2_decode_status(py_status, sizeof(py_status), &out) != NBP2_OK) { return 21; }
        printf("status_from_py=%d,%.3f,%.3f,%u,%u,%u,%u,%d\n", (int)out.state,
               (double)out.valence, (double)out.arousal, (unsigned)out.heap_free,
               (unsigned)out.psram_free, (unsigned)out.fps_x10, (unsigned)out.bus_dropped,
               (int)out.rssi);
    }
    {
        static const uint8_t py_timer_set[] = {'''
        + c_byte_array(py_encoded["timer_set"])
        + r'''};
        nbp2_msg_timer_set_t out = {0};
        if (nbp2_decode_timer_set(py_timer_set, sizeof(py_timer_set), &out) != NBP2_OK) { return 22; }
        printf("timer_set_from_py=%u,%llu,%s\n", (unsigned)out.timer_id,
               (unsigned long long)out.fire_at_unix_ms, out.label);
    }

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

    py_hello = nbp2.Hello(proto_major=0, proto_minor=1, boot_id=0x11223344, caps=7, token=b"abcd")
    py_heartbeat = nbp2.Heartbeat(counter=42, uptime_ms=123456789012)
    py_status = nbp2.Status(
        state=nbp2.FsmState.IDLE,
        valence=0.5,
        arousal=-0.25,
        heap_free=1000,
        psram_free=2000,
        fps_x10=300,
        bus_dropped=0,
        rssi=-42,
    )
    py_timer_set = nbp2.TimerSet(timer_id=1, fire_at_unix_ms=1700000000000, label="lembrete")
    py_event_state = nbp2.EventState(from_=nbp2.FsmState.IDLE, to=nbp2.FsmState.SLEEPING)

    py_encoded = {
        "hello": nbp2.encode_hello(py_hello),
        "heartbeat": nbp2.encode_heartbeat(py_heartbeat),
        "status": nbp2.encode_status(py_status),
        "timer_set": nbp2.encode_timer_set(py_timer_set),
        "event_state": nbp2.encode_event_state(py_event_state),
    }

    c_source = BUILD / "nbp2_golden.c"
    exe = BUILD / ("nbp2_golden.exe" if os.name == "nt" else "nbp2_golden")
    write_c_golden_source(c_source, py_encoded)

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
        "hello_frame": nbp2.encode_frame(nbp2.MSG_HELLO, 0x01020304, hello_payload).hex(),
        "heartbeat_frame": nbp2.encode_frame(nbp2.MSG_HEARTBEAT, 7, b"").hex(),
        "token_equal": "1" if nbp2.timing_safe_equal(b"\x01\x02\x03\x04", b"\x01\x02\x03\x04") else "0",
        "token_diff": "1" if nbp2.timing_safe_equal(b"\x01\x02\x03\x04", b"\x01\x02\x03\x05") else "0",
        "token_len": "1" if nbp2.timing_safe_equal(b"\x01\x02\x03\x04", b"\x01\x02\x03") else "0",
        "hello_cbor": py_encoded["hello"].hex(),
        "heartbeat_cbor": py_encoded["heartbeat"].hex(),
        "status_cbor": py_encoded["status"].hex(),
        "timer_set_cbor": py_encoded["timer_set"].hex(),
        "event_state_cbor": py_encoded["event_state"].hex(),
        "hello_from_py": f"0,1,{0x11223344},7,4:61626364",
        "status_from_py": "1,0.500,-0.250,1000,2000,300,0,-42",
        "timer_set_from_py": "1,1700000000000,lembrete",
    }

    if c_values != expected:
        print("protocol-golden: divergencia C/Python")
        for key in sorted(set(c_values) | set(expected)):
            c_val = c_values.get(key)
            py_val = expected.get(key)
            if c_val != py_val:
                print(f"  {key}: C={c_val!r} PY={py_val!r}")
        return 1

    print("protocol-golden: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
