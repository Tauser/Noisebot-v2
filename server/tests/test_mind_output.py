from __future__ import annotations

import asyncio
from collections.abc import AsyncIterator

import pytest

from noisebot2.bus import EventBus
from noisebot2.mind import (
    MindOutput,
    ReplyReady,
    SayAudio,
    SayBegin,
    SayCancel,
    SayEnd,
    SentenceReady,
    SpeechCancellationRequested,
    SpeechDone,
)


async def _wait_for_event(
    queue: asyncio.Queue[object],
    event_type: type[object],
    *,
    timeout: float = 1.0,
):
    while True:
        event = await asyncio.wait_for(queue.get(), timeout=timeout)
        if isinstance(event, event_type):
            return event


@pytest.mark.asyncio
async def test_mind_output_emits_sentence_then_say_sequence() -> None:
    bus = EventBus()

    async def tts_provider(_text: str) -> AsyncIterator[bytes]:
        yield b"a"
        yield b"b"

    actor = MindOutput(bus, tts_provider=tts_provider)
    events = bus.subscribe(SentenceReady, SayBegin, SayAudio, SayEnd, SpeechDone, maxsize=64)
    await actor.start()

    try:
        await bus.publish(ReplyReady(turn_id=7, text="ola mundo", source="llm"))

        sentence = await _wait_for_event(events, SentenceReady)
        begin = await _wait_for_event(events, SayBegin)
        audio0 = await _wait_for_event(events, SayAudio)
        audio1 = await _wait_for_event(events, SayAudio)
        end = await _wait_for_event(events, SayEnd)
        done = await _wait_for_event(events, SpeechDone)

        assert sentence.turn_id == 7
        assert sentence.sentence == "ola mundo"
        assert begin.turn_id == 7
        assert audio0.turn_id == 7
        assert audio0.index == 0
        assert audio0.pcm == b"a"
        assert audio1.turn_id == 7
        assert audio1.index == 1
        assert audio1.pcm == b"b"
        assert end.turn_id == 7
        assert done.turn_id == 7
    finally:
        await actor.shutdown()


@pytest.mark.asyncio
async def test_mind_output_cancel_emits_say_cancel_without_speech_done() -> None:
    bus = EventBus()
    release_audio = asyncio.Event()

    async def tts_provider(_text: str) -> AsyncIterator[bytes]:
        yield b"primeiro"
        await release_audio.wait()
        yield b"segundo"

    actor = MindOutput(bus, tts_provider=tts_provider)
    events = bus.subscribe(SayBegin, SayAudio, SayCancel, SpeechDone, maxsize=64)
    await actor.start()

    try:
        await bus.publish(ReplyReady(turn_id=9, text="resposta longa", source="llm"))

        begin = await _wait_for_event(events, SayBegin)
        audio = await _wait_for_event(events, SayAudio)
        assert begin.turn_id == 9
        assert audio.turn_id == 9

        await bus.publish(SpeechCancellationRequested(turn_id=9, reason="barge_in"))

        cancelled = await _wait_for_event(events, SayCancel)
        assert cancelled.turn_id == 9
        assert cancelled.reason == "barge_in"

        await asyncio.sleep(0.05)
        while not events.empty():
            event = events.get_nowait()
            assert not isinstance(event, SpeechDone)
    finally:
        release_audio.set()
        await actor.shutdown()


@pytest.mark.asyncio
async def test_mind_output_tts_error_emits_cancel_and_speech_done() -> None:
    bus = EventBus()

    async def tts_provider(_text: str) -> AsyncIterator[bytes]:
        raise RuntimeError("tts offline")
        yield b""

    actor = MindOutput(bus, tts_provider=tts_provider)
    events = bus.subscribe(SentenceReady, SayBegin, SayCancel, SpeechDone, maxsize=64)
    await actor.start()

    try:
        await bus.publish(ReplyReady(turn_id=11, text="resposta", source="llm"))

        sentence = await _wait_for_event(events, SentenceReady)
        begin = await _wait_for_event(events, SayBegin)
        cancelled = await _wait_for_event(events, SayCancel)
        done = await _wait_for_event(events, SpeechDone)

        assert sentence.turn_id == 11
        assert begin.turn_id == 11
        assert cancelled.turn_id == 11
        assert cancelled.reason == "tts_error"
        assert done.turn_id == 11
    finally:
        await actor.shutdown()
