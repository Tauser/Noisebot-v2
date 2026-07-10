"""Smoke host-side dos providers da mente."""

from __future__ import annotations

from dataclasses import dataclass

from noisebot2.providers import (
    AbstractLlmProvider,
    AbstractSttProvider,
    AbstractTtsProvider,
    SttTranscript,
)


@dataclass(frozen=True)
class ProviderSmokeResult:
    provider: str
    ok: bool
    detail: str


@dataclass(frozen=True)
class ProviderSmokeReport:
    llm: ProviderSmokeResult
    tts: ProviderSmokeResult
    stt: ProviderSmokeResult

    @property
    def ok(self) -> bool:
        return self.llm.ok and self.tts.ok and self.stt.ok


async def run_provider_smoke(
    *,
    llm_provider: AbstractLlmProvider | None,
    tts_provider: AbstractTtsProvider | None,
    stt_provider: AbstractSttProvider | None,
    prompt: str = "Responda com uma frase curta em portugues do Brasil.",
) -> ProviderSmokeReport:
    llm_result, llm_text = await _smoke_llm(llm_provider, prompt=prompt)
    tts_result, pcm = await _smoke_tts(tts_provider, text=llm_text or "teste de voz")
    stt_result = await _smoke_stt(stt_provider, pcm=pcm)
    return ProviderSmokeReport(llm=llm_result, tts=tts_result, stt=stt_result)


async def _smoke_llm(
    provider: AbstractLlmProvider | None,
    *,
    prompt: str,
) -> tuple[ProviderSmokeResult, str]:
    if provider is None:
        return ProviderSmokeResult("llm", False, "provider nao configurado"), ""
    try:
        text = await provider.generate_reply(prompt, {})
    except Exception as exc:
        return ProviderSmokeResult("llm", False, str(exc)), ""
    if not text.strip():
        return ProviderSmokeResult("llm", False, "resposta vazia"), ""
    return ProviderSmokeResult("llm", True, text[:120]), text


async def _smoke_tts(
    provider: AbstractTtsProvider | None,
    *,
    text: str,
) -> tuple[ProviderSmokeResult, bytes]:
    if provider is None:
        return ProviderSmokeResult("tts", False, "provider nao configurado"), b""
    try:
        rendered = bytearray()
        async for chunk in provider.synthesize(text):
            rendered.extend(chunk)
    except Exception as exc:
        return ProviderSmokeResult("tts", False, str(exc)), b""
    if not rendered:
        return ProviderSmokeResult("tts", False, "audio vazio"), b""
    return ProviderSmokeResult("tts", True, f"{len(rendered)} bytes"), bytes(rendered)


async def _smoke_stt(
    provider: AbstractSttProvider | None,
    *,
    pcm: bytes,
) -> ProviderSmokeResult:
    if provider is None:
        return ProviderSmokeResult("stt", False, "provider nao configurado")
    if not pcm:
        return ProviderSmokeResult("stt", False, "sem pcm para transcrever")
    try:
        transcript = await provider.transcribe_pcm16le(pcm)
    except Exception as exc:
        return ProviderSmokeResult("stt", False, str(exc))
    return _transcript_result(transcript)


def _transcript_result(transcript: SttTranscript) -> ProviderSmokeResult:
    text = transcript.text.strip()
    if not text:
        return ProviderSmokeResult("stt", False, "transcricao vazia")
    return ProviderSmokeResult("stt", True, f"{transcript.language}:{text[:120]}")
