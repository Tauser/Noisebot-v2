#!/usr/bin/env python3
"""Fake NBP/2 server for bench-testing the mind_link shell (S1.7).

Not part of the product — a throwaway tool to validate the robot's TCP
client against something that speaks the real wire format, without needing
the full Python server (mind_link comes before the mente exists).

Usage:
    python protocol/codegen/generate_nbp2.py   # se ainda nao gerou
    python tools/nbp2_fake_server.py [--port 9100] [--drop-after SECONDS]
"""

from __future__ import annotations

import argparse
import socket
import sys
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "protocol" / "generated" / "python"))

import nbp2  # noqa: E402  (path inserted above)


def recv_frame(conn: socket.socket, buf: bytearray) -> nbp2.FrameView | None:
    while True:
        if len(buf) >= 3 and buf[0] == nbp2.SOF:
            payload_len = int.from_bytes(buf[1:3], "little")
            total_len = nbp2.FRAME_OVERHEAD + payload_len
            if len(buf) >= total_len:
                frame = bytes(buf[:total_len])
                del buf[:total_len]
                return nbp2.decode_frame(frame)
        elif len(buf) >= 1 and buf[0] != nbp2.SOF:
            del buf[0:1]
            continue

        chunk = conn.recv(4096)
        if not chunk:
            return None
        buf += chunk


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=9100)
    parser.add_argument(
        "--drop-after", type=float, default=None, help="derruba a conexao apos N segundos (soak)"
    )
    args = parser.parse_args()

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listener.bind((args.host, args.port))
        listener.listen(1)
        print(f"nbp2-fake-server: escutando em {args.host}:{args.port}")

        while True:
            conn, addr = listener.accept()
            print(f"nbp2-fake-server: conexao de {addr}")
            handle_connection(conn, args.drop_after)
            print("nbp2-fake-server: conexao encerrada, voltando a escutar")


def handle_connection(conn: socket.socket, drop_after: float | None) -> None:
    buf = bytearray()
    conn.settimeout(0.5)
    started_at = time.monotonic()
    boot_id = None

    with conn:
        while True:
            if drop_after is not None and time.monotonic() - started_at > drop_after:
                print("nbp2-fake-server: drop-after atingido, derrubando conexao de proposito")
                return

            try:
                view = recv_frame(conn, buf)
            except socket.timeout:
                continue
            except ValueError as exc:
                print(f"nbp2-fake-server: frame invalido ({exc}), encerrando")
                return

            if view is None:
                print("nbp2-fake-server: robo fechou a conexao")
                return

            if view.msg_type == nbp2.MSG_HELLO:
                hello = nbp2.decode_hello(view.payload)
                boot_id = hello.boot_id
                print(
                    f"nbp2-fake-server: HELLO boot_id={hello.boot_id:#x} "
                    f"proto={hello.proto_major}.{hello.proto_minor} "
                    f"token_len={len(hello.token)}"
                )
                ack = nbp2.HelloAck(
                    proto_major=nbp2.PROTO_MAJOR,
                    proto_minor=nbp2.PROTO_MINOR,
                    boot_id=hello.boot_id,
                    caps=0,
                )
                conn.sendall(
                    nbp2.encode_frame(nbp2.MSG_HELLO_ACK, view.seq, nbp2.encode_hello_ack(ack))
                )
                print("nbp2-fake-server: HELLO_ACK enviado, sessao READY")
            elif view.msg_type == nbp2.MSG_HEARTBEAT:
                hb = nbp2.decode_heartbeat(view.payload)
                print(f"nbp2-fake-server: HEARTBEAT recebido counter={hb.counter}")
                reply = nbp2.Heartbeat(counter=hb.counter, uptime_ms=hb.uptime_ms)
                conn.sendall(
                    nbp2.encode_frame(nbp2.MSG_HEARTBEAT, view.seq, nbp2.encode_heartbeat(reply))
                )
            else:
                print(f"nbp2-fake-server: frame tipo 0x{view.msg_type:04x} ignorado")


if __name__ == "__main__":
    raise SystemExit(main())
