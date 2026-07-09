from __future__ import annotations

import asyncio

import pytest

from noisebot2.bus import EventBus
from noisebot2.mind import FinalTranscript, SentenceReady, SessionStarted, SpeechDone, WakeDetected
from noisebot2.providers import AbstractLlmProvider, CircuitBreaker, LlmProviderError, StaticTtsProvider
from noisebot2.runtime import MindRuntime


class FlakyLlmProvider(AbstractLlmProvider):
    def __init__(self) -> None:
        self.breaker = CircuitBreaker(provider="llm/flaky", failure_threshold=2, reset_timeout=60.0)
        self.calls = 0

    async def generate_reply(self, text: str, context: dict[str, object]) -> str:
        del text, context
        self.breaker.allow_request()
        self.calls += 1
        self.breaker.record_failure()
        raise LlmProviderError("llm indisponivel")


async def _wait_for_event(queue: asyncio.Queue[object], event_type: type[object], timeout: float = 1.0):
    while True:
        event = await asyncio.wait_for(queue.get(), timeout=timeout)
        if isinstance(event, event_type):
            return event


@pytest.mark.asyncio
async def test_runtime_degrades_honestly_when_llm_provider_fails() -> None:
    bus = EventBus()
    runtime = MindRuntime(
        bus=bus,
        llm_provider=FlakyLlmProvider(),
        tts_provider=StaticTtsProvider(b"fala"),
    )
    events = bus.subscribe(SessionStarted, SentenceReady, SpeechDone, maxsize=64)
    await runtime.start()

    try:
        await bus.publish(WakeDetected())
        started = await _wait_for_event(events, SessionStarted)
        await bus.publish(FinalTranscript(turn_id=started.turn_id, text="me responde"))

        sentence = await _wait_for_event(events, SentenceReady)
        done = await _wait_for_event(events, SpeechDone)

        assert sentence.turn_id == started.turn_id
        assert "Nao consegui pensar agora" in sentence.sentence
        assert done.turn_id == started.turn_id
    finally:
        await runtime.shutdown()


@pytest.mark.asyncio
async def test_runtime_short_circuits_when_breaker_is_open() -> None:
    bus = EventBus()
    provider = FlakyLlmProvider()
    runtime = MindRuntime(
        bus=bus,
        llm_provider=provider,
        tts_provider=StaticTtsProvider(b"fala"),
    )
    events = bus.subscribe(SessionStarted, SentenceReady, SpeechDone, maxsize=128)
    await runtime.start()

    try:
        seen_turn_ids: list[int] = []
        for expected_turn_id in (1, 2):
            await bus.publish(WakeDetected())
            started = await _wait_for_event(events, SessionStarted)
            await bus.publish(FinalTranscript(turn_id=started.turn_id, text="teste"))
            sentence = await _wait_for_event(events, SentenceReady)
            done = await _wait_for_event(events, SpeechDone)
            assert sentence.turn_id == started.turn_id
            assert done.turn_id == started.turn_id
            seen_turn_ids.append(started.turn_id)

        assert seen_turn_ids[1] > seen_turn_ids[0]

        assert provider.breaker.state.value == "open"
        assert provider.calls == 2

        await bus.publish(WakeDetected())
        started = await _wait_for_event(events, SessionStarted)
        await bus.publish(FinalTranscript(turn_id=started.turn_id, text="terceiro"))
        sentence = await _wait_for_event(events, SentenceReady)
        done = await _wait_for_event(events, SpeechDone)

        assert sentence.turn_id == started.turn_id
        assert "Nao consegui pensar agora" in sentence.sentence
        assert done.turn_id == started.turn_id
        assert provider.calls == 2
    finally:
        await runtime.shutdown()
