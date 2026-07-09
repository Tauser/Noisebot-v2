from __future__ import annotations

import asyncio
from collections.abc import AsyncIterator

import pytest

from noisebot2.bus import EventBus
from noisebot2.mind import (
    BargeInDetected,
    FinalTranscript,
    IntentResolved,
    SayBegin,
    SayCancel,
    SentenceReady,
    SessionStarted,
    SpeechDone,
    TurnCancelled,
    TurnState,
    TurnStateChanged,
    WakeDetected,
)
from noisebot2.runtime import MindRuntime


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
async def test_turn_i1_new_turn_cancels_previous_turn_completely() -> None:
    bus = EventBus()
    gate = asyncio.Event()

    async def llm_provider(_text: str, _session) -> str:
        await gate.wait()
        return "resposta lenta"

    runtime = MindRuntime(bus=bus, llm_provider=llm_provider)
    events = bus.subscribe(SessionStarted, TurnCancelled, TurnStateChanged, maxsize=64)
    await runtime.start()

    try:
        await bus.publish(WakeDetected())
        started = await _wait_for_event(events, SessionStarted)
        assert runtime.turn_engine.state == TurnState.LISTENING

        await bus.publish(FinalTranscript(turn_id=started.turn_id, text="primeiro turno"))

        thinking = await _wait_for_event(events, TurnStateChanged)
        while thinking.new_state != TurnState.THINKING:
            thinking = await _wait_for_event(events, TurnStateChanged)

        await bus.publish(WakeDetected())

        cancelled = await _wait_for_event(events, TurnCancelled)
        restarted = await _wait_for_event(events, SessionStarted)

        assert cancelled.turn_id == started.turn_id
        assert cancelled.reason == "barge_in"
        assert restarted.turn_id != started.turn_id
        assert restarted.reason == "barge_in"
        assert runtime.turn_engine.current_turn_id == restarted.turn_id
        assert runtime.turn_engine.state == TurnState.LISTENING
    finally:
        gate.set()
        await runtime.shutdown()


@pytest.mark.asyncio
async def test_turn_i2_barge_in_cancels_task_during_speaking() -> None:
    bus = EventBus()
    speaking_started = asyncio.Event()
    release_audio = asyncio.Event()

    async def llm_provider(_text: str, _session) -> str:
        return "fala longa"

    async def tts_provider(_text: str) -> AsyncIterator[bytes]:
        yield b"chunk-0"
        speaking_started.set()
        await release_audio.wait()
        yield b"chunk-1"

    runtime = MindRuntime(bus=bus, llm_provider=llm_provider, tts_provider=tts_provider)
    events = bus.subscribe(SayBegin, SayCancel, SessionStarted, SpeechDone, maxsize=64)
    await runtime.start()

    try:
        await bus.publish(WakeDetected())
        started = await _wait_for_event(events, SessionStarted)
        await bus.publish(FinalTranscript(turn_id=started.turn_id, text="interrompa"))

        say_begin = await _wait_for_event(events, SayBegin)
        assert say_begin.turn_id == started.turn_id
        await asyncio.wait_for(speaking_started.wait(), timeout=1.0)

        await bus.publish(BargeInDetected(turn_id=started.turn_id))

        cancelled = await _wait_for_event(events, SayCancel)
        restarted = await _wait_for_event(events, SessionStarted)

        assert cancelled.turn_id == started.turn_id
        assert restarted.turn_id != started.turn_id
        assert runtime.turn_engine.state == TurnState.LISTENING
        assert runtime.turn_engine.current_turn_id == restarted.turn_id

        await asyncio.sleep(0.05)
        while not events.empty():
            event = events.get_nowait()
            assert not isinstance(event, SpeechDone) or event.turn_id != started.turn_id
    finally:
        release_audio.set()
        await runtime.shutdown()


@pytest.mark.asyncio
async def test_turn_wake_during_speaking_publishes_barge_in_signal() -> None:
    bus = EventBus()
    speaking_started = asyncio.Event()
    hold_tts = asyncio.Event()

    async def llm_provider(_text: str, _session) -> str:
        return "fala em andamento"

    async def tts_provider(_text: str):
        yield b"chunk-0"
        speaking_started.set()
        await hold_tts.wait()
        yield b"chunk-1"

    runtime = MindRuntime(bus=bus, llm_provider=llm_provider, tts_provider=tts_provider)
    events = bus.subscribe(SessionStarted, SayBegin, BargeInDetected, maxsize=64)
    await runtime.start()

    try:
        await bus.publish(WakeDetected())
        started = await _wait_for_event(events, SessionStarted)
        await bus.publish(FinalTranscript(turn_id=started.turn_id, text="continue"))

        begin = await _wait_for_event(events, SayBegin)
        assert begin.turn_id == started.turn_id
        await asyncio.wait_for(speaking_started.wait(), timeout=1.0)

        await bus.publish(WakeDetected())

        barge = await _wait_for_event(events, BargeInDetected)
        assert barge.turn_id == started.turn_id
    finally:
        hold_tts.set()
        await runtime.shutdown()


@pytest.mark.asyncio
async def test_turn_i4_provider_failure_degrades_with_honest_reply() -> None:
    bus = EventBus()

    async def llm_provider(_text: str, _session) -> str:
        raise RuntimeError("llm offline")

    runtime = MindRuntime(bus=bus, llm_provider=llm_provider)
    events = bus.subscribe(SessionStarted, SentenceReady, SpeechDone, TurnStateChanged, maxsize=64)
    await runtime.start()

    try:
        await bus.publish(WakeDetected())
        started = await _wait_for_event(events, SessionStarted)
        await bus.publish(FinalTranscript(turn_id=started.turn_id, text="fala comigo"))

        sentence = await _wait_for_event(events, SentenceReady)
        done = await _wait_for_event(events, SpeechDone)
        idle = await _wait_for_event(events, TurnStateChanged)
        while idle.new_state != TurnState.IDLE:
            idle = await _wait_for_event(events, TurnStateChanged)

        assert sentence.turn_id == started.turn_id
        assert "Nao consegui pensar agora" in sentence.sentence
        assert done.turn_id == started.turn_id
        assert idle.turn_id == started.turn_id
        assert runtime.turn_engine.state == TurnState.IDLE
    finally:
        await runtime.shutdown()


@pytest.mark.asyncio
async def test_turn_i5_local_intent_runs_before_llm() -> None:
    bus = EventBus()
    llm_calls = 0

    async def llm_provider(_text: str, _session) -> str:
        nonlocal llm_calls
        llm_calls += 1
        return "nao deveria ser chamado"

    async def intent_resolver(_text: str, _session) -> IntentResolved:
        return IntentResolved(
            turn_id=_session.turn_id,
            intent_name="local_status",
            reply_text="Estou operacional.",
            resolution_reason="offline_first",
        )

    runtime = MindRuntime(
        bus=bus,
        intent_resolver=intent_resolver,
        llm_provider=llm_provider,
    )
    events = bus.subscribe(SessionStarted, SentenceReady, SpeechDone, maxsize=64)
    await runtime.start()

    try:
        await bus.publish(WakeDetected())
        started = await _wait_for_event(events, SessionStarted)
        await bus.publish(FinalTranscript(turn_id=started.turn_id, text="status"))

        sentence = await _wait_for_event(events, SentenceReady)
        done = await _wait_for_event(events, SpeechDone)

        assert sentence.turn_id == started.turn_id
        assert sentence.sentence == "Estou operacional."
        assert done.turn_id == started.turn_id
        assert llm_calls == 0
    finally:
        await runtime.shutdown()
