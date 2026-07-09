from __future__ import annotations

import pytest

from noisebot2.runtime import MindRuntime


class ClosableProvider:
    def __init__(self) -> None:
        self.closed = False

    async def close(self) -> None:
        self.closed = True


@pytest.mark.asyncio
async def test_runtime_shutdown_closes_configured_providers() -> None:
    llm = ClosableProvider()
    stt = ClosableProvider()
    tts = ClosableProvider()
    runtime = MindRuntime(llm_provider=llm, stt_provider=stt, tts_provider=tts)

    await runtime.start()
    await runtime.shutdown()

    assert llm.closed is True
    assert stt.closed is True
    assert tts.closed is True
