#!/usr/bin/env python3
"""Fake NBP/2 server for bench-testing the mind_link shell (S1.7).

Not part of the product — a throwaway tool to validate the robot's TCP
client against something that speaks the real wire format, without needing
the full Python server (mind_link comes before the mente exists).

Usage:
    python protocol/codegen/generate_nbp2.py   # se ainda nao gerou
    python tools/nbp2_fake_server.py [--port 9100] [--drop-after SECONDS]
                                     [--max-cycles N] [--require-token HEX]
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
    parser.add_argument(
        "--max-cycles",
        type=int,
        default=None,
        help="encerra o server apos N HELLOs bem-sucedidos (soak de reconexao)",
    )
    parser.add_argument(
        "--require-token",
        default=None,
        help="hex do token esperado; HELLO com token diferente e recusado (conexao encerrada "
        "sem HELLO_ACK), como o servidor real faria (PROTOCOL.md S3)",
    )
    parser.add_argument(
        "--send-say-after-listen-end",
        action="store_true",
        help="envia um turno SAY_* sintetico apos o primeiro LISTEN_END recebido",
    )
    args = parser.parse_args()
    required_token = bytes.fromhex(args.require_token) if args.require_token else None

    cycles_ok = 0
    cycles_total = 0

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listener.bind((args.host, args.port))
        listener.listen(1)
        print(f"nbp2-fake-server: escutando em {args.host}:{args.port}")

        while True:
            conn, addr = listener.accept()
            cycles_total += 1
            print(f"nbp2-fake-server: conexao de {addr} (ciclo {cycles_total})")
            got_hello_ack = handle_connection(
                conn, args.drop_after, required_token, args.send_say_after_listen_end
            )
            if got_hello_ack:
                cycles_ok += 1
            print(
                f"nbp2-fake-server: conexao encerrada "
                f"(ok={cycles_ok}/{cycles_total}), voltando a escutar"
            )

            if args.max_cycles is not None and cycles_total >= args.max_cycles:
                print(
                    f"nbp2-fake-server: soak concluido: {cycles_ok}/{cycles_total} "
                    "ciclos com HELLO_ACK"
                )
                return 0 if cycles_ok == cycles_total else 1


def handle_connection(
    conn: socket.socket,
    drop_after: float | None,
    required_token: bytes | None,
    send_say_after_listen_end: bool,
) -> bool:
    buf = bytearray()
    conn.settimeout(0.5)
    started_at = time.monotonic()
    boot_id = None
    got_hello_ack = False
    sent_say = False

    with conn:
        while True:
            if drop_after is not None and time.monotonic() - started_at > drop_after:
                print("nbp2-fake-server: drop-after atingido, derrubando conexao de proposito")
                return got_hello_ack

            try:
                view = recv_frame(conn, buf)
            except socket.timeout:
                continue
            except ValueError as exc:
                print(f"nbp2-fake-server: frame invalido ({exc}), encerrando")
                return got_hello_ack

            if view is None:
                print("nbp2-fake-server: robo fechou a conexao")
                return got_hello_ack

            if view.msg_type == nbp2.MSG_HELLO:
                hello = nbp2.decode_hello(view.payload)
                boot_id = hello.boot_id
                print(
                    f"nbp2-fake-server: HELLO boot_id={hello.boot_id:#x} "
                    f"proto={hello.proto_major}.{hello.proto_minor} "
                    f"token_len={len(hello.token)}"
                )

                if required_token is not None and not nbp2.timing_safe_equal(
                    hello.token, required_token
                ):
                    print("nbp2-fake-server: token incorreto/ausente, recusando (sem HELLO_ACK)")
                    return got_hello_ack

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
                got_hello_ack = True
            elif view.msg_type == nbp2.MSG_HEARTBEAT:
                hb = nbp2.decode_heartbeat(view.payload)
                print(f"nbp2-fake-server: HEARTBEAT recebido counter={hb.counter}")
                reply = nbp2.Heartbeat(counter=hb.counter, uptime_ms=hb.uptime_ms)
                conn.sendall(
                    nbp2.encode_frame(nbp2.MSG_HEARTBEAT, view.seq, nbp2.encode_heartbeat(reply))
                )
            elif view.msg_type == nbp2.MSG_EVENT_WAKE:
                wake = nbp2.decode_event_wake(view.payload)
                print(f"nbp2-fake-server: EVENT_WAKE score={wake.score:.2f}")
            elif view.msg_type == nbp2.MSG_LISTEN_START:
                listen = nbp2.decode_listen_start(view.payload)
                print(
                    "nbp2-fake-server: LISTEN_START "
                    f"session_id={listen.session_id} sample_rate={listen.sample_rate}"
                )
            elif view.msg_type == nbp2.MSG_LISTEN_AUDIO:
                audio = nbp2.decode_listen_audio(view.payload)
                print(
                    "nbp2-fake-server: LISTEN_AUDIO "
                    f"session_id={audio.session_id} bytes={len(audio.pcm)}"
                )
            elif view.msg_type == nbp2.MSG_LISTEN_END:
                listen = nbp2.decode_listen_end(view.payload)
                print(f"nbp2-fake-server: LISTEN_END session_id={listen.session_id}")
                if send_say_after_listen_end and not sent_say:
                    send_demo_say(conn, turn_id=1, sample_rate=16000)
                    sent_say = True
            else:
                print(f"nbp2-fake-server: frame tipo 0x{view.msg_type:04x} ignorado")


def send_demo_say(conn: socket.socket, turn_id: int, sample_rate: int) -> None:
    chunk_a = bytes([0x00, 0x10] * 64)
    chunk_b = bytes([0x00, 0xF0] * 64)

    print(
        "nbp2-fake-server: enviando turno SAY sintetico "
        f"turn_id={turn_id} sample_rate={sample_rate}"
    )
    conn.sendall(
        nbp2.encode_frame(
            nbp2.MSG_SAY_BEGIN,
            100,
            nbp2.encode_say_begin(nbp2.SayBegin(turn_id=turn_id, sample_rate=sample_rate)),
        )
    )
    conn.sendall(
        nbp2.encode_frame(
            nbp2.MSG_SAY_AUDIO,
            101,
            nbp2.encode_say_audio(nbp2.SayAudio(turn_id=turn_id, pcm=chunk_a)),
        )
    )
    conn.sendall(
        nbp2.encode_frame(
            nbp2.MSG_SAY_AUDIO,
            102,
            nbp2.encode_say_audio(nbp2.SayAudio(turn_id=turn_id, pcm=chunk_b)),
        )
    )
    conn.sendall(
        nbp2.encode_frame(
            nbp2.MSG_SAY_END,
            103,
            nbp2.encode_say_end(nbp2.SayEnd(turn_id=turn_id)),
        )
    )


if __name__ == "__main__":
    raise SystemExit(main())
