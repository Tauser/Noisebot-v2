"""Smoke host-side dos providers da mente."""

from __future__ import annotations

import argparse
import asyncio
import importlib.util
import json
import shutil
import wave
from dataclasses import dataclass
from pathlib import Path
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


@dataclass(frozen=True)
class ProviderDoctorReport:
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
    tts_text: str = "teste de voz",
    stt_pcm: bytes | None = None,
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
        tts_input = llm_text if "llm" in resolved_axes else tts_text
        tts_result, pcm = await _smoke_tts(tts_provider, text=tts_input or "teste de voz")
    elif stt_pcm is not None:
        pcm = stt_pcm
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
    tts_text: str = "teste de voz",
    stt_wav_path: str | None = None,
) -> ProviderSmokeReport:
    load_dotenv(dotenv_path)
    llm_provider = build_llm_provider(load_llm_config())
    tts_provider = build_tts_provider(load_tts_config())
    stt_provider = build_stt_provider(load_stt_config())
    stt_pcm = _load_wav_pcm16le(stt_wav_path) if stt_wav_path else None
    try:
        return await run_provider_smoke(
            llm_provider=llm_provider,
            tts_provider=tts_provider,
            stt_provider=stt_provider,
            prompt=prompt,
            active_axes=active_axes,
            tts_text=tts_text,
            stt_pcm=stt_pcm,
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


def format_provider_doctor_report(report: ProviderDoctorReport) -> str:
    status = "OK" if report.ok else "FAIL"
    lines = [f"provider doctor: {status} ({', '.join(report.active_axes)})"]
    for result in (report.llm, report.tts, report.stt):
        provider_status = "OK" if result.ok else "FAIL"
        lines.append(f"- {result.provider}: {provider_status} - {result.detail}")
    return "\n".join(lines)


def run_provider_doctor_from_env(
    *,
    dotenv_path: str | None = None,
    active_axes: Sequence[ProviderAxis] = DEFAULT_AXES,
    stt_wav_path: str | None = None,
) -> ProviderDoctorReport:
    load_dotenv(dotenv_path)
    resolved_axes = _normalize_axes(active_axes)
    llm_config = load_llm_config()
    tts_config = load_tts_config()
    stt_config = load_stt_config()
    llm_result = _doctor_llm(llm_config) if "llm" in resolved_axes else ProviderSmokeResult("llm", False, "skipped")
    tts_result = _doctor_tts(tts_config) if "tts" in resolved_axes else ProviderSmokeResult("tts", False, "skipped")
    stt_result = (
        _doctor_stt(stt_config, stt_wav_path=stt_wav_path)
        if "stt" in resolved_axes
        else ProviderSmokeResult("stt", False, "skipped")
    )
    return ProviderDoctorReport(
        llm=llm_result,
        tts=tts_result,
        stt=stt_result,
        active_axes=resolved_axes,
    )


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
    parser.add_argument(
        "--text",
        default="teste de voz",
        help="Texto usado no smoke de TTS quando LLM nao participa.",
    )
    parser.add_argument(
        "--wav",
        dest="stt_wav_path",
        default=None,
        help="Arquivo WAV mono PCM16LE usado no smoke de STT quando TTS nao participa.",
    )
    parser.add_argument(
        "--doctor",
        action="store_true",
        help="Valida pre-requisitos/configuracao dos providers sem executar o fluxo completo.",
    )
    return parser


async def _run_cli(argv: Sequence[str] | None = None) -> int:
    args = _build_arg_parser().parse_args(list(argv) if argv is not None else None)
    active_axes = tuple(args.only_axes) if args.only_axes else DEFAULT_AXES
    if args.doctor:
        report = run_provider_doctor_from_env(
            dotenv_path=args.dotenv_path,
            active_axes=active_axes,
            stt_wav_path=args.stt_wav_path,
        )
        if args.json:
            print(json.dumps(report.to_dict(), ensure_ascii=True))
        else:
            print(format_provider_doctor_report(report))
        return 0 if report.ok else 1
    report = await run_provider_smoke_from_env(
        dotenv_path=args.dotenv_path,
        prompt=args.prompt,
        active_axes=active_axes,
        tts_text=args.text,
        stt_wav_path=args.stt_wav_path,
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


def _load_wav_pcm16le(path: str) -> bytes:
    wav_path = Path(path)
    if not wav_path.exists():
        raise FileNotFoundError(f"arquivo WAV nao encontrado: {wav_path}")
    with wave.open(str(wav_path), "rb") as wav_file:
        channels = wav_file.getnchannels()
        sample_width = wav_file.getsampwidth()
        if channels != 1 or sample_width != 2:
            raise ValueError("WAV deve ser mono PCM16LE")
        return wav_file.readframes(wav_file.getnframes())


def _doctor_llm(config) -> ProviderSmokeResult:
    provider = config.provider.value
    if provider == "none":
        return ProviderSmokeResult("llm", False, "provider nao configurado")
    if provider == "lmstudio":
        return ProviderSmokeResult("llm", True, f"lmstudio configurado em {config.lmstudio_base_url}")
    if provider == "ollama":
        return ProviderSmokeResult("llm", True, f"ollama configurado em {config.ollama_base_url}")
    if provider == "openai":
        ok = bool(config.openai_api_key)
        detail = "OPENAI_API_KEY configurada" if ok else "OPENAI_API_KEY ausente"
        return ProviderSmokeResult("llm", ok, detail)
    if provider == "api_chat":
        ok = bool(config.api_chat_base_url and config.api_chat_api_key)
        detail = "api_chat configurado" if ok else "NOISEBOT_API_CHAT_BASE_URL/API_KEY ausentes"
        return ProviderSmokeResult("llm", ok, detail)
    if provider == "anthropic":
        ok = bool(config.anthropic_api_key)
        detail = "ANTHROPIC_API_KEY configurada" if ok else "ANTHROPIC_API_KEY ausente"
        return ProviderSmokeResult("llm", ok, detail)
    return ProviderSmokeResult("llm", True, f"{provider} configurado")


def _doctor_tts(config) -> ProviderSmokeResult:
    if config.provider.value == "none":
        return ProviderSmokeResult("tts", False, "provider nao configurado")
    executable = _resolve_executable(config.piper_executable)
    if executable is None:
        return ProviderSmokeResult("tts", False, f"executavel Piper nao encontrado: {config.piper_executable}")
    model_path = Path(config.piper_model) if config.piper_model else None
    if model_path is None or not model_path.exists():
        return ProviderSmokeResult("tts", False, f"modelo Piper nao encontrado: {config.piper_model or '<vazio>'}")
    return ProviderSmokeResult("tts", True, f"piper pronto ({executable}; modelo {model_path.name})")


def _doctor_stt(config, *, stt_wav_path: str | None) -> ProviderSmokeResult:
    if config.provider.value == "none":
        return ProviderSmokeResult("stt", False, "provider nao configurado")
    if importlib.util.find_spec("faster_whisper") is None:
        return ProviderSmokeResult("stt", False, "faster-whisper nao instalado")
    if stt_wav_path:
        try:
            _load_wav_pcm16le(stt_wav_path)
        except Exception as exc:
            return ProviderSmokeResult("stt", False, str(exc))
        return ProviderSmokeResult("stt", True, f"faster-whisper pronto; wav valido: {Path(stt_wav_path).name}")
    return ProviderSmokeResult("stt", True, "faster-whisper pronto; faltando apenas WAV de entrada")


def _resolve_executable(executable: str) -> str | None:
    if not executable:
        return None
    candidate = Path(executable)
    if candidate.is_file():
        return str(candidate)
    return shutil.which(executable)


if __name__ == "__main__":
    raise SystemExit(main())
