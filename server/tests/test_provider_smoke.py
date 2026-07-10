from __future__ import annotations

import pytest

from noisebot2.provider_smoke import run_provider_smoke
from noisebot2.providers import AbstractSttProvider, StaticLlmProvider, StaticTtsProvider, SttTranscript


class StaticSttProvider(AbstractSttProvider):
    async def transcribe_pcm16le(self, pcm: bytes, *, sample_rate_hz: int = 16000) -> SttTranscript:
        del sample_rate_hz
        return SttTranscript(text=f"pcm {len(pcm)}", language="pt")


class FailingSttProvider(AbstractSttProvider):
    async def transcribe_pcm16le(self, pcm: bytes, *, sample_rate_hz: int = 16000) -> SttTranscript:
        del pcm, sample_rate_hz
        raise RuntimeError("stt offline")


@pytest.mark.asyncio
async def test_provider_smoke_reports_success_for_llm_tts_stt() -> None:
    report = await run_provider_smoke(
        llm_provider=StaticLlmProvider("fala curta"),
        tts_provider=StaticTtsProvider(b"\x01\x02", b"\x03\x04"),
        stt_provider=StaticSttProvider(),
    )

    assert report.ok is True
    assert report.llm.ok is True
    assert report.tts.ok is True
    assert report.stt.ok is True
    assert report.stt.detail == "pt:pcm 4"


@pytest.mark.asyncio
async def test_provider_smoke_reports_explicit_failures() -> None:
    report = await run_provider_smoke(
        llm_provider=StaticLlmProvider("fala curta"),
        tts_provider=StaticTtsProvider(b"\x01\x02"),
        stt_provider=FailingSttProvider(),
    )

    assert report.ok is False
    assert report.llm.ok is True
    assert report.tts.ok is True
    assert report.stt.ok is False
    assert report.stt.detail == "stt offline"
