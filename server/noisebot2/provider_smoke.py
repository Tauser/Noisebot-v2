"""Smoke host-side dos providers da mente."""

from __future__ import annotations

import argparse
import asyncio
import json
from dataclasses import dataclass
from typing import Literal, Sequence

from noisebot2.config import load_dotenv, load_llm_config, load_stt_config, load_tts_config
from noisebot2.providers import build_llm_provider, build_stt_provider, build_tts_provider

from noisebot2.providers import (
    AbstractLlmProvider,
    AbstractSttProvider,
    AbstractTtsProvider,
    SttTranscript,
)

ProviderAxis = Literal["llm", "tts", "stt"]
DEFAULT_AXES: tuple[ProviderAxis, ...] = ("llm", "tts", "stt")


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
    active_axes: tuple[ProviderAxis, ...] = DEFAULT_AXES

    @property
    def ok(self) -> bool:
        results = {
            "llm": self.llm.ok,
            "tts": self.tts.ok,
            "stt": self.stt.ok,
        }
        return all(results[axis] for axis in self.active_axes)

    def to_dict(self) -> dict[str, object]:
        return {
            "ok": self.ok,
            "active_axes": list(self.active_axes),
            "llm": _result_to_dict(self.llm),
            "tts": _result_to_dict(self.tts),
            "stt": _result_to_dict(self.stt),
        }


async def run_provider_smoke(
    *,
    llm_provider: AbstractLlmProvider | None,
    tts_provider: AbstractTtsProvider | None,
    stt_provider: AbstractSttProvider | None,
    prompt: str = "Responda com uma frase curta em portugues do Brasil.",
    active_axes: Sequence[ProviderAxis] = DEFAULT_AXES,
) -> ProviderSmokeReport:
    resolved_axes = _normalize_axes(active_axes)
    llm_result = ProviderSmokeResult("llm", False, "skipped")
    tts_result = ProviderSmokeResult("tts", False, "skipped")
    stt_result = ProviderSmokeResult("stt", False, "skipped")
    llm_text = ""
    pcm = b""

    if "llm" in resolved_axes:
        llm_result, llm_text = await _smoke_llm(llm_provider, prompt=prompt)
    if "tts" in resolved_axes:
        tts_text = llm_text if "llm" in resolved_axes else "teste de voz"
        tts_result, pcm = await _smoke_tts(tts_provider, text=tts_text or "teste de voz")
    if "stt" in resolved_axes:
        stt_result = await _smoke_stt(stt_provider, pcm=pcm)
    return ProviderSmokeReport(
        llm=llm_result,
        tts=tts_result,
        stt=stt_result,
        active_axes=resolved_axes,
    )


async def run_provider_smoke_from_env(
    *,
    dotenv_path: str | None = None,
    prompt: str = "Responda com uma frase curta em portugues do Brasil.",
    active_axes: Sequence[ProviderAxis] = DEFAULT_AXES,
) -> ProviderSmokeReport:
    load_dotenv(dotenv_path)
    llm_provider = build_llm_provider(load_llm_config())
    tts_provider = build_tts_provider(load_tts_config())
    stt_provider = build_stt_provider(load_stt_config())
    try:
        return await run_provider_smoke(
            llm_provider=llm_provider,
            tts_provider=tts_provider,
            stt_provider=stt_provider,
            prompt=prompt,
            active_axes=active_axes,
        )
    finally:
        await _close_provider(llm_provider)
        await _close_provider(tts_provider)
        await _close_provider(stt_provider)


def format_provider_smoke_report(report: ProviderSmokeReport) -> str:
    status = "OK" if report.ok else "FAIL"
    lines = [f"provider smoke: {status} ({', '.join(report.active_axes)})"]
    for result in (report.llm, report.tts, report.stt):
        provider_status = "OK" if result.ok else "FAIL"
        lines.append(f"- {result.provider}: {provider_status} - {result.detail}")
    return "\n".join(lines)


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


def _result_to_dict(result: ProviderSmokeResult) -> dict[str, object]:
    return {
        "provider": result.provider,
        "ok": result.ok,
        "detail": result.detail,
    }


async def _close_provider(provider: object | None) -> None:
    if provider is None:
        return
    close = getattr(provider, "close", None)
    if close is None:
        return
    await close()


def _build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Smoke host-side dos providers do NoiseBot 2.")
    parser.add_argument("--dotenv", dest="dotenv_path", default=None, help="Caminho opcional para .env")
    parser.add_argument(
        "--prompt",
        default="Responda com uma frase curta em portugues do Brasil.",
        help="Prompt curto usado no smoke do LLM",
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="Emite o relatorio em JSON em vez de texto legivel.",
    )
    parser.add_argument(
        "--only",
        choices=DEFAULT_AXES,
        action="append",
        dest="only_axes",
        help="Restringe o smoke a um eixo especifico; repita para combinar eixos.",
    )
    return parser


async def _run_cli(argv: Sequence[str] | None = None) -> int:
    args = _build_arg_parser().parse_args(list(argv) if argv is not None else None)
    active_axes = tuple(args.only_axes) if args.only_axes else DEFAULT_AXES
    report = await run_provider_smoke_from_env(
        dotenv_path=args.dotenv_path,
        prompt=args.prompt,
        active_axes=active_axes,
    )
    if args.json:
        print(json.dumps(report.to_dict(), ensure_ascii=True))
    else:
        print(format_provider_smoke_report(report))
    return 0 if report.ok else 1


def main(argv: Sequence[str] | None = None) -> int:
    return asyncio.run(_run_cli(argv))


def _normalize_axes(active_axes: Sequence[ProviderAxis]) -> tuple[ProviderAxis, ...]:
    if not active_axes:
        return DEFAULT_AXES
    deduped: list[ProviderAxis] = []
    for axis in active_axes:
        if axis not in deduped:
            deduped.append(axis)
    return tuple(deduped)


if __name__ == "__main__":
    raise SystemExit(main())
