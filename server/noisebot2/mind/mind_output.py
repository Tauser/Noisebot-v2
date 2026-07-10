"""Ator dono da fala de saída da mente."""

from __future__ import annotations

import asyncio
import inspect
from collections.abc import AsyncIterator, Awaitable, Callable

from noisebot2.bus import EventBus
from noisebot2.providers.tts import AbstractTtsProvider

from .contracts import (
    ReplyReady,
    SayAudio,
    SayBegin,
    SayCancel,
    SayEnd,
    SentenceReady,
    SpeechCancellationRequested,
    SpeechDone,
    TurnBudgetReported,
)

TtsProvider = Callable[[str], AsyncIterator[bytes] | Awaitable[AsyncIterator[bytes]]]


class MindOutput:
    """Sentenciza e agenda SAY_* sem ser chamado diretamente por outro ator."""

    def __init__(self, bus: EventBus, *, tts_provider: TtsProvider | None = None) -> None:
        self._bus = bus
        self._tts_provider = tts_provider
        self._events = bus.subscribe(ReplyReady, SpeechCancellationRequested)
        self._runner: asyncio.Task[None] | None = None
        self._active_turn_id = 0
        self._speak_task: asyncio.Task[None] | None = None

    async def start(self) -> None:
        if self._runner is None:
            self._runner = asyncio.create_task(self.run())

    async def shutdown(self) -> None:
        if self._speak_task is not None and not self._speak_task.done():
            self._speak_task.cancel()
            try:
                await self._speak_task
            except asyncio.CancelledError:
                pass
        if self._runner is not None:
            self._runner.cancel()
            try:
                await self._runner
            except asyncio.CancelledError:
                pass
            self._runner = None

    async def run(self) -> None:
        async for event in self._bus.iter_queue(self._events):
            if isinstance(event, ReplyReady):
                await self._on_reply_ready(event)
            elif isinstance(event, SpeechCancellationRequested):
                await self._on_cancel(event)

    async def _on_reply_ready(self, event: ReplyReady) -> None:
        if self._speak_task is not None and not self._speak_task.done():
            self._speak_task.cancel()
            try:
                await self._speak_task
            except asyncio.CancelledError:
                pass
        self._active_turn_id = event.turn_id
        self._speak_task = asyncio.create_task(self._speak(event))

    async def _on_cancel(self, event: SpeechCancellationRequested) -> None:
        if event.turn_id != self._active_turn_id:
            return
        if self._speak_task is not None and not self._speak_task.done():
            self._speak_task.cancel()
            try:
                await self._speak_task
            except asyncio.CancelledError:
                pass

    async def _speak(self, event: ReplyReady) -> None:
        await self._bus.publish(SentenceReady(turn_id=event.turn_id, sentence=event.text, index=0))
        say_begin = SayBegin(turn_id=event.turn_id)
        await self._bus.publish(say_begin)

        try:
            index = 0
            first_audio_t: float | None = None
            async for chunk in self._iter_tts_chunks(event.text):
                say_audio = SayAudio(turn_id=event.turn_id, pcm=chunk, index=index)
                await self._bus.publish(say_audio)
                if first_audio_t is None:
                    first_audio_t = say_audio.t
                index += 1
            await self._bus.publish(SayEnd(turn_id=event.turn_id))
            await self._bus.publish(SpeechDone(turn_id=event.turn_id))
            await self._publish_budget_report(event, say_begin.t, first_audio_t)
        except asyncio.CancelledError:
            await self._bus.publish(SayCancel(turn_id=event.turn_id, reason="barge_in"))
            raise
        except Exception:
            await self._bus.publish(SayCancel(turn_id=event.turn_id, reason="tts_error"))
            await self._bus.publish(SpeechDone(turn_id=event.turn_id))
            await self._publish_budget_report(event, say_begin.t, None)
        finally:
            if self._active_turn_id == event.turn_id:
                self._active_turn_id = 0

    async def _publish_budget_report(
        self,
        event: ReplyReady,
        say_begin_t: float,
        first_audio_t: float | None,
    ) -> None:
        speech_to_first_audio_ms = None
        reply_to_first_audio_ms = None
        if first_audio_t is not None:
            speech_to_first_audio_ms = max(0, int((first_audio_t - event.t) * 1000))
            reply_to_first_audio_ms = max(0, int((first_audio_t - say_begin_t) * 1000))
        end_of_turn_ms = max(0, int((asyncio.get_running_loop().time() - event.t) * 1000))
        await self._bus.publish(
            TurnBudgetReported(
                turn_id=event.turn_id,
                source=event.source,
                speech_to_first_audio_ms=speech_to_first_audio_ms,
                reply_to_first_audio_ms=reply_to_first_audio_ms,
                end_of_turn_ms=end_of_turn_ms,
            )
        )

    async def _iter_tts_chunks(self, text: str) -> AsyncIterator[bytes]:
        if isinstance(self._tts_provider, AbstractTtsProvider):
            async for chunk in self._tts_provider.synthesize(text):
                yield chunk
            return

        if self._tts_provider is None:
            yield text.encode("utf-8")
            return

        iterator = self._tts_provider(text)
        if inspect.isawaitable(iterator):
            iterator = await iterator

        async for chunk in iterator:
            yield chunk
