from __future__ import annotations

import asyncio
import socket
import sys
from pathlib import Path

import pytest

from noisebot2.bus import EventBus
from noisebot2.config import Nbp2TransportConfig
from noisebot2.mind import QuietModeSetRequested, TimerSetRequested, VolumeSetRequested
from noisebot2.transport import Nbp2TransportServer

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "protocol" / "generated" / "python"))

import nbp2  # noqa: E402


def _pick_free_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.bind(("127.0.0.1", 0))
        return int(sock.getsockname()[1])


async def _recv_frame(reader: asyncio.StreamReader) -> nbp2.FrameView:
    header = await reader.readexactly(3)
    while header[:1] != bytes([nbp2.SOF]):
        header = header[1:] + await reader.readexactly(1)
    payload_len = int.from_bytes(header[1:3], "little")
    rest = await reader.readexactly(nbp2.FRAME_OVERHEAD + payload_len - 3)
    return nbp2.decode_frame(header + rest)


@pytest.mark.asyncio
async def test_nbp2_transport_acknowledges_hello_and_sends_control_messages() -> None:
    bus = EventBus()
    port = _pick_free_port()
    transport = Nbp2TransportServer(
        bus,
        config=Nbp2TransportConfig(bind_host="127.0.0.1", bind_port=port, robot_token=b"abcd"),
    )
    await transport.start()

    try:
        reader, writer = await asyncio.open_connection("127.0.0.1", port)
        hello = nbp2.Hello(
            proto_major=nbp2.PROTO_MAJOR,
            proto_minor=nbp2.PROTO_MINOR,
            boot_id=123,
            caps=0,
            token=b"abcd",
        )
        writer.write(nbp2.encode_frame(nbp2.MSG_HELLO, 1, nbp2.encode_hello(hello)))
        await writer.drain()

        hello_ack = await _recv_frame(reader)
        assert hello_ack.msg_type == nbp2.MSG_HELLO_ACK
        time_sync = await _recv_frame(reader)
        assert time_sync.msg_type == nbp2.MSG_TIME_SYNC

        await bus.publish(
            TimerSetRequested(
                turn_id=7,
                timer_id=7,
                fire_at_unix_ms=1_234_567_890,
                label="timer 5 minutos",
                duration_ms=300000,
            )
        )

        timer_set = await _recv_frame(reader)
        assert timer_set.msg_type == nbp2.MSG_TIMER_SET
        decoded = nbp2.decode_timer_set(timer_set.payload)
        assert decoded.timer_id == 7
        assert decoded.fire_at_unix_ms == 1_234_567_890
        assert decoded.label == "timer 5 minutos"

        await bus.publish(VolumeSetRequested(turn_id=7, volume_percent=40))

        volume_set = await _recv_frame(reader)
        assert volume_set.msg_type == nbp2.MSG_VOLUME_SET
        decoded_volume = nbp2.decode_volume_set(volume_set.payload)
        assert decoded_volume.volume_percent == 40

        await bus.publish(QuietModeSetRequested(turn_id=7, enabled=True))

        quiet_set = await _recv_frame(reader)
        assert quiet_set.msg_type == nbp2.MSG_QUIET_MODE_SET
        decoded_quiet = nbp2.decode_quiet_mode_set(quiet_set.payload)
        assert decoded_quiet.enabled == 1

        writer.close()
        await writer.wait_closed()
    finally:
        await transport.shutdown()


@pytest.mark.asyncio
async def test_nbp2_transport_rejects_wrong_token_without_hello_ack() -> None:
    bus = EventBus()
    port = _pick_free_port()
    transport = Nbp2TransportServer(
        bus,
        config=Nbp2TransportConfig(bind_host="127.0.0.1", bind_port=port, robot_token=b"abcd"),
    )
    await transport.start()

    try:
        reader, writer = await asyncio.open_connection("127.0.0.1", port)
        hello = nbp2.Hello(
            proto_major=nbp2.PROTO_MAJOR,
            proto_minor=nbp2.PROTO_MINOR,
            boot_id=123,
            caps=0,
            token=b"zzzz",
        )
        writer.write(nbp2.encode_frame(nbp2.MSG_HELLO, 1, nbp2.encode_hello(hello)))
        await writer.drain()

        with pytest.raises((asyncio.IncompleteReadError, ConnectionResetError)):
            await asyncio.wait_for(_recv_frame(reader), timeout=1.0)

        writer.close()
        await writer.wait_closed()
    finally:
        await transport.shutdown()
