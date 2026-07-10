"""Servidor NBP/2 mínimo para a mente do NoiseBot 2."""

from __future__ import annotations

import asyncio
import sys
import time
from dataclasses import dataclass
from pathlib import Path

from noisebot2.bus import EventBus
from noisebot2.config import Nbp2TransportConfig
from noisebot2.mind import QuietModeSetRequested, TimerSetRequested, VolumeSetRequested

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "protocol" / "generated" / "python"))

import nbp2  # noqa: E402


@dataclass
class _ClientSession:
    writer: asyncio.StreamWriter
    boot_id: int
    next_seq: int = 1
    ready: bool = False


class Nbp2TransportServer:
    """Borda fina NBP/2: autentica o robô e envia comandos da mente."""

    def __init__(self, bus: EventBus, *, config: Nbp2TransportConfig) -> None:
        self._bus = bus
        self._config = config
        self._events = bus.subscribe(TimerSetRequested, VolumeSetRequested, QuietModeSetRequested)
        self._server: asyncio.base_events.Server | None = None
        self._runner: asyncio.Task[None] | None = None
        self._client: _ClientSession | None = None
        self._client_mux = asyncio.Lock()

    async def start(self) -> None:
        if self._server is None:
            self._server = await asyncio.start_server(
                self._handle_client,
                host=self._config.bind_host,
                port=self._config.bind_port,
            )
        if self._runner is None:
            self._runner = asyncio.create_task(self._run())

    async def shutdown(self) -> None:
        if self._runner is not None:
            self._runner.cancel()
            try:
                await self._runner
            except asyncio.CancelledError:
                pass
            self._runner = None
        if self._server is not None:
            self._server.close()
            await self._server.wait_closed()
            self._server = None
        async with self._client_mux:
            if self._client is not None:
                self._client.writer.close()
                await self._client.writer.wait_closed()
                self._client = None

    async def _run(self) -> None:
        async for event in self._bus.iter_queue(self._events):
            if isinstance(event, TimerSetRequested):
                await self._send_timer_set(event)
            elif isinstance(event, VolumeSetRequested):
                await self._send_volume_set(event)
            elif isinstance(event, QuietModeSetRequested):
                await self._send_quiet_mode_set(event)

    async def _handle_client(
        self,
        reader: asyncio.StreamReader,
        writer: asyncio.StreamWriter,
    ) -> None:
        session = _ClientSession(writer=writer, boot_id=0)
        try:
            while True:
                frame = await _read_frame(reader)
                if frame is None:
                    return
                if frame.msg_type == nbp2.MSG_HELLO:
                    hello = nbp2.decode_hello(frame.payload)
                    if self._config.robot_token and not nbp2.timing_safe_equal(
                        hello.token,
                        self._config.robot_token,
                    ):
                        return
                    session.boot_id = hello.boot_id
                    ack = nbp2.HelloAck(
                        proto_major=nbp2.PROTO_MAJOR,
                        proto_minor=nbp2.PROTO_MINOR,
                        boot_id=hello.boot_id,
                        caps=0,
                    )
                    await _write_frame(
                        writer,
                        nbp2.MSG_HELLO_ACK,
                        frame.seq,
                        nbp2.encode_hello_ack(ack),
                    )
                    time_sync = nbp2.TimeSync(
                        unix_ms=int(time.time() * 1000),
                        server_mono_ms=int(time.monotonic() * 1000),
                    )
                    await _write_frame(
                        writer,
                        nbp2.MSG_TIME_SYNC,
                        session.next_seq,
                        nbp2.encode_time_sync(time_sync),
                    )
                    session.next_seq += 1
                    session.ready = True
                    async with self._client_mux:
                        if self._client is not None and self._client.writer is not writer:
                            self._client.writer.close()
                            await self._client.writer.wait_closed()
                        self._client = session
                elif frame.msg_type == nbp2.MSG_HEARTBEAT:
                    heartbeat = nbp2.decode_heartbeat(frame.payload)
                    await _write_frame(
                        writer,
                        nbp2.MSG_HEARTBEAT,
                        frame.seq,
                        nbp2.encode_heartbeat(heartbeat),
                    )
        except asyncio.IncompleteReadError:
            return
        finally:
            async with self._client_mux:
                if self._client is not None and self._client.writer is writer:
                    self._client = None
            writer.close()
            await writer.wait_closed()

    async def _send_timer_set(self, event: TimerSetRequested) -> None:
        async with self._client_mux:
            client = self._client
            if client is None or not client.ready:
                return
            payload = nbp2.encode_timer_set(
                nbp2.TimerSet(
                    timer_id=event.timer_id,
                    fire_at_unix_ms=event.fire_at_unix_ms,
                    label=event.label,
                )
            )
            await _write_frame(client.writer, nbp2.MSG_TIMER_SET, client.next_seq, payload)
            client.next_seq += 1

    async def _send_volume_set(self, event: VolumeSetRequested) -> None:
        async with self._client_mux:
            client = self._client
            if client is None or not client.ready:
                return
            payload = nbp2.encode_volume_set(
                nbp2.VolumeSet(volume_percent=event.volume_percent)
            )
            await _write_frame(client.writer, nbp2.MSG_VOLUME_SET, client.next_seq, payload)
            client.next_seq += 1

    async def _send_quiet_mode_set(self, event: QuietModeSetRequested) -> None:
        async with self._client_mux:
            client = self._client
            if client is None or not client.ready:
                return
            payload = nbp2.encode_quiet_mode_set(
                nbp2.QuietModeSet(enabled=1 if event.enabled else 0)
            )
            await _write_frame(client.writer, nbp2.MSG_QUIET_MODE_SET, client.next_seq, payload)
            client.next_seq += 1


async def _read_frame(reader: asyncio.StreamReader) -> nbp2.FrameView | None:
    header = await reader.readexactly(3)
    while header[:1] != bytes([nbp2.SOF]):
        header = header[1:] + await reader.readexactly(1)
    payload_len = int.from_bytes(header[1:3], "little")
    rest = await reader.readexactly(nbp2.FRAME_OVERHEAD + payload_len - 3)
    return nbp2.decode_frame(header + rest)


async def _write_frame(
    writer: asyncio.StreamWriter,
    msg_type: int,
    seq: int,
    payload: bytes,
) -> None:
    writer.write(nbp2.encode_frame(msg_type, seq, payload))
    await writer.drain()
