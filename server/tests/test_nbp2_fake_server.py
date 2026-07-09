"""Testes host-side do fake server NBP/2 para S4.3.

Valida o menor fluxo ponta a ponta já disponível sem hardware:
HELLO/HELLO_ACK, uplink LISTEN_* e downlink SAY_* sintetizado pelo
`tools/nbp2_fake_server.py`.
"""

from __future__ import annotations

import socket
import subprocess
import sys
import time
from pathlib import Path

import pytest


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "protocol" / "generated" / "python"))

import nbp2  # noqa: E402


def _pick_free_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.bind(("127.0.0.1", 0))
        return int(sock.getsockname()[1])


def _recv_frame(conn: socket.socket, buf: bytearray) -> nbp2.FrameView | None:
    while True:
        if len(buf) >= 3 and buf[0] == nbp2.SOF:
            payload_len = int.from_bytes(buf[1:3], "little")
            total_len = nbp2.FRAME_OVERHEAD + payload_len
            if len(buf) >= total_len:
                frame = bytes(buf[:total_len])
                del buf[:total_len]
                return nbp2.decode_frame(frame)
        elif len(buf) >= 1 and buf[0] != nbp2.SOF:
            del buf[:1]
            continue

        chunk = conn.recv(4096)
        if not chunk:
            return None
        buf += chunk


def _wait_for_server(port: int, timeout_s: float = 5.0) -> None:
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.2):
                return
        except OSError:
            time.sleep(0.05)
    raise AssertionError(f"fake server nao abriu a porta {port} a tempo")


def _send_frame(conn: socket.socket, msg_type: int, seq: int, payload: bytes) -> None:
    frame = nbp2.encode_frame(msg_type, seq, payload)
    conn.sendall(frame)


@pytest.mark.parametrize(
    ("say_mode", "expected_last_type"),
    [
        ("end", nbp2.MSG_SAY_END),
        ("cancel", nbp2.MSG_SAY_CANCEL),
        ("drop", None),
    ],
)
def test_fake_server_replies_with_say_turn_after_listen_end(say_mode: str, expected_last_type: int | None):
    port = _pick_free_port()
    server = subprocess.Popen(
        [
            sys.executable,
            str(ROOT / "tools" / "nbp2_fake_server.py"),
            "--host",
            "127.0.0.1",
            "--port",
            str(port),
            "--send-say-after-listen-end",
            "--say-mode",
            say_mode,
        ],
        cwd=ROOT,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )

    try:
        _wait_for_server(port)

        with socket.create_connection(("127.0.0.1", port), timeout=2.0) as conn:
            conn.settimeout(2.0)
            buf = bytearray()

            hello = nbp2.Hello(
                proto_major=nbp2.PROTO_MAJOR,
                proto_minor=nbp2.PROTO_MINOR,
                boot_id=0x12345678,
                caps=0,
                token=b"",
            )
            _send_frame(conn, nbp2.MSG_HELLO, 1, nbp2.encode_hello(hello))

            hello_ack = _recv_frame(conn, buf)
            assert hello_ack is not None
            assert hello_ack.msg_type == nbp2.MSG_HELLO_ACK
            ack = nbp2.decode_hello_ack(hello_ack.payload)
            assert ack.boot_id == hello.boot_id

            _send_frame(
                conn,
                nbp2.MSG_LISTEN_START,
                2,
                nbp2.encode_listen_start(nbp2.ListenStart(session_id=7, sample_rate=16000)),
            )
            _send_frame(
                conn,
                nbp2.MSG_LISTEN_AUDIO,
                3,
                nbp2.encode_listen_audio(nbp2.ListenAudio(session_id=7, pcm=bytes([1, 2] * 64))),
            )
            _send_frame(
                conn,
                nbp2.MSG_LISTEN_END,
                4,
                nbp2.encode_listen_end(nbp2.ListenEnd(session_id=7)),
            )

            frame = _recv_frame(conn, buf)
            assert frame is not None
            assert frame.msg_type == nbp2.MSG_SAY_BEGIN
            say_begin = nbp2.decode_say_begin(frame.payload)
            assert say_begin.turn_id == 1
            assert say_begin.sample_rate == 16000

            audio_a = _recv_frame(conn, buf)
            audio_b = _recv_frame(conn, buf)
            assert audio_a is not None and audio_a.msg_type == nbp2.MSG_SAY_AUDIO
            assert audio_b is not None and audio_b.msg_type == nbp2.MSG_SAY_AUDIO
            assert nbp2.decode_say_audio(audio_a.payload).turn_id == 1
            assert nbp2.decode_say_audio(audio_b.payload).turn_id == 1

            last_frame = _recv_frame(conn, buf)
            if expected_last_type is None:
                assert last_frame is None
            else:
                assert last_frame is not None
                assert last_frame.msg_type == expected_last_type
    finally:
        server.terminate()
        try:
            server.wait(timeout=2.0)
        except subprocess.TimeoutExpired:
            server.kill()
            server.wait(timeout=2.0)
