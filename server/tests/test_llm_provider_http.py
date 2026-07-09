from __future__ import annotations

import pytest

from noisebot2.providers import ChatCompletionsLlmProvider


class FakeResponse:
    def __init__(self, payload: dict[str, object], status: int = 200) -> None:
        self._payload = payload
        self.status = status

    async def __aenter__(self) -> "FakeResponse":
        return self

    async def __aexit__(self, exc_type, exc, tb) -> None:
        return None

    async def json(self) -> dict[str, object]:
        return self._payload

    async def text(self) -> str:
        return str(self._payload)


class FakeSession:
    def __init__(self, payload: dict[str, object]) -> None:
        self._payload = payload
        self.post_calls = 0
        self.closed = False

    def post(self, url: str, *, headers=None, json=None):
        del url, headers, json
        self.post_calls += 1
        return FakeResponse(self._payload)

    async def close(self) -> None:
        self.closed = True


@pytest.mark.asyncio
async def test_chat_completions_provider_reuses_session_between_requests() -> None:
    created_sessions: list[FakeSession] = []

    def session_factory(*, timeout):
        del timeout
        session = FakeSession({"choices": [{"message": {"content": "oi"}}]})
        created_sessions.append(session)
        return session

    provider = ChatCompletionsLlmProvider(
        provider_label="test",
        model="demo",
        api_key="sk-test",
        session_factory=session_factory,
    )

    first = await provider.generate_reply("fala", {})
    second = await provider.generate_reply("fala de novo", {})

    assert first == "oi"
    assert second == "oi"
    assert len(created_sessions) == 1
    assert created_sessions[0].post_calls == 2

    await provider.close()
    assert created_sessions[0].closed is True


@pytest.mark.asyncio
async def test_chat_completions_provider_reopens_session_after_close() -> None:
    created_sessions: list[FakeSession] = []

    def session_factory(*, timeout):
        del timeout
        session = FakeSession({"choices": [{"message": {"content": "ok"}}]})
        created_sessions.append(session)
        return session

    provider = ChatCompletionsLlmProvider(
        provider_label="test",
        model="demo",
        api_key="sk-test",
        session_factory=session_factory,
    )

    await provider.generate_reply("primeiro", {})
    await provider.close()
    await provider.generate_reply("segundo", {})

    assert len(created_sessions) == 2
    assert created_sessions[0].closed is True
    assert created_sessions[1].closed is False
