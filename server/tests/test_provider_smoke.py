from __future__ import annotations

import wave
from pathlib import Path

import pytest

from noisebot2.provider_smoke import (
    ProviderSmokeReport,
    ProviderSmokeResult,
    ProviderDoctorReport,
    format_provider_doctor_report,
    format_provider_smoke_report,
    main,
    run_provider_smoke,
    run_provider_doctor_from_env,
    run_provider_smoke_from_env,
)
from noisebot2.providers import AbstractSttProvider, StaticLlmProvider, StaticTtsProvider, SttTranscript


class StaticSttProvider(AbstractSttProvider):
    async def transcribe_pcm16le(self, pcm: bytes, *, sample_rate_hz: int = 16000) -> SttTranscript:
        del sample_rate_hz
        return SttTranscript(text=f"pcm {len(pcm)}", language="pt")


class FailingSttProvider(AbstractSttProvider):
    async def transcribe_pcm16le(self, pcm: bytes, *, sample_rate_hz: int = 16000) -> SttTranscript:
        del pcm, sample_rate_hz
        raise RuntimeError("stt offline")


class ClosableProvider:
    def __init__(self) -> None:
        self.closed = False

    async def close(self) -> None:
        self.closed = True


def _write_test_wav(path: Path, frames: bytes) -> None:
    with wave.open(str(path), "wb") as wav_file:
        wav_file.setnchannels(1)
        wav_file.setsampwidth(2)
        wav_file.setframerate(16000)
        wav_file.writeframes(frames)


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


@pytest.mark.asyncio
async def test_provider_smoke_can_scope_to_llm_only() -> None:
    report = await run_provider_smoke(
        llm_provider=StaticLlmProvider("fala curta"),
        tts_provider=None,
        stt_provider=None,
        active_axes=("llm",),
    )

    assert report.ok is True
    assert report.active_axes == ("llm",)
    assert report.llm.ok is True
    assert report.tts.detail == "skipped"
    assert report.stt.detail == "skipped"


@pytest.mark.asyncio
async def test_provider_smoke_can_scope_to_tts_only_with_explicit_text() -> None:
    report = await run_provider_smoke(
        llm_provider=None,
        tts_provider=StaticTtsProvider(b"\x01\x02"),
        stt_provider=None,
        active_axes=("tts",),
        tts_text="teste dedicado",
    )

    assert report.ok is True
    assert report.active_axes == ("tts",)
    assert report.llm.detail == "skipped"
    assert report.tts.ok is True
    assert report.stt.detail == "skipped"


@pytest.mark.asyncio
async def test_provider_smoke_from_env_builds_and_closes_providers(monkeypatch) -> None:
    llm = StaticLlmProvider("fala curta")
    tts = StaticTtsProvider(b"\x01\x02")
    stt = StaticSttProvider()
    llm_close = ClosableProvider()
    tts_close = ClosableProvider()
    stt_close = ClosableProvider()
    llm.close = llm_close.close  # type: ignore[method-assign]
    tts.close = tts_close.close  # type: ignore[method-assign]
    stt.close = stt_close.close  # type: ignore[method-assign]

    monkeypatch.setattr("noisebot2.provider_smoke.load_dotenv", lambda path=None: None)
    monkeypatch.setattr("noisebot2.provider_smoke.load_llm_config", lambda: object())
    monkeypatch.setattr("noisebot2.provider_smoke.load_tts_config", lambda: object())
    monkeypatch.setattr("noisebot2.provider_smoke.load_stt_config", lambda: object())
    monkeypatch.setattr("noisebot2.provider_smoke.build_llm_provider", lambda _config: llm)
    monkeypatch.setattr("noisebot2.provider_smoke.build_tts_provider", lambda _config: tts)
    monkeypatch.setattr("noisebot2.provider_smoke.build_stt_provider", lambda _config: stt)

    report = await run_provider_smoke_from_env(prompt="teste")

    assert report.ok is True
    assert llm_close.closed is True
    assert tts_close.closed is True
    assert stt_close.closed is True


@pytest.mark.asyncio
async def test_provider_smoke_from_env_passes_active_axes(monkeypatch) -> None:
    async def fake_run_provider_smoke(**kwargs) -> ProviderSmokeReport:
        assert kwargs["active_axes"] == ("llm",)
        return ProviderSmokeReport(
            llm=ProviderSmokeResult("llm", True, "ok"),
            tts=ProviderSmokeResult("tts", False, "skipped"),
            stt=ProviderSmokeResult("stt", False, "skipped"),
            active_axes=("llm",),
        )

    monkeypatch.setattr("noisebot2.provider_smoke.load_dotenv", lambda path=None: None)
    monkeypatch.setattr("noisebot2.provider_smoke.load_llm_config", lambda: object())
    monkeypatch.setattr("noisebot2.provider_smoke.load_tts_config", lambda: object())
    monkeypatch.setattr("noisebot2.provider_smoke.load_stt_config", lambda: object())
    monkeypatch.setattr("noisebot2.provider_smoke.build_llm_provider", lambda _config: None)
    monkeypatch.setattr("noisebot2.provider_smoke.build_tts_provider", lambda _config: None)
    monkeypatch.setattr("noisebot2.provider_smoke.build_stt_provider", lambda _config: None)
    monkeypatch.setattr("noisebot2.provider_smoke.run_provider_smoke", fake_run_provider_smoke)

    report = await run_provider_smoke_from_env(active_axes=("llm",))

    assert report.ok is True
    assert report.active_axes == ("llm",)


@pytest.mark.asyncio
async def test_provider_smoke_from_env_passes_stt_wav_pcm(monkeypatch, tmp_path: Path) -> None:
    wav_path = tmp_path / "sample.wav"
    _write_test_wav(wav_path, b"\x01\x02\x03\x04")

    async def fake_run_provider_smoke(**kwargs) -> ProviderSmokeReport:
        assert kwargs["active_axes"] == ("stt",)
        assert kwargs["stt_pcm"] == b"\x01\x02\x03\x04"
        return ProviderSmokeReport(
            llm=ProviderSmokeResult("llm", False, "skipped"),
            tts=ProviderSmokeResult("tts", False, "skipped"),
            stt=ProviderSmokeResult("stt", True, "pt:teste"),
            active_axes=("stt",),
        )

    monkeypatch.setattr("noisebot2.provider_smoke.load_dotenv", lambda path=None: None)
    monkeypatch.setattr("noisebot2.provider_smoke.load_llm_config", lambda: object())
    monkeypatch.setattr("noisebot2.provider_smoke.load_tts_config", lambda: object())
    monkeypatch.setattr("noisebot2.provider_smoke.load_stt_config", lambda: object())
    monkeypatch.setattr("noisebot2.provider_smoke.build_llm_provider", lambda _config: None)
    monkeypatch.setattr("noisebot2.provider_smoke.build_tts_provider", lambda _config: None)
    monkeypatch.setattr("noisebot2.provider_smoke.build_stt_provider", lambda _config: None)
    monkeypatch.setattr("noisebot2.provider_smoke.run_provider_smoke", fake_run_provider_smoke)

    report = await run_provider_smoke_from_env(active_axes=("stt",), stt_wav_path=str(wav_path))

    assert report.ok is True
    assert report.active_axes == ("stt",)


def test_format_provider_smoke_report_is_human_readable() -> None:
    report = ProviderSmokeReport(
        llm=ProviderSmokeResult("llm", True, "ok"),
        tts=ProviderSmokeResult("tts", False, "tts offline"),
        stt=ProviderSmokeResult("stt", False, "sem pcm"),
    )

    assert format_provider_smoke_report(report) == (
        "provider smoke: FAIL (llm, tts, stt)\n"
        "- llm: OK - ok\n"
        "- tts: FAIL - tts offline\n"
        "- stt: FAIL - sem pcm"
    )


def test_format_provider_doctor_report_is_human_readable() -> None:
    report = ProviderDoctorReport(
        llm=ProviderSmokeResult("llm", True, "lmstudio configurado"),
        tts=ProviderSmokeResult("tts", False, "modelo ausente"),
        stt=ProviderSmokeResult("stt", False, "skipped"),
        active_axes=("llm", "tts"),
    )

    assert format_provider_doctor_report(report) == (
        "provider doctor: FAIL (llm, tts)\n"
        "- llm: OK - lmstudio configurado\n"
        "- tts: FAIL - modelo ausente\n"
        "- stt: FAIL - skipped"
    )


def test_provider_doctor_from_env_checks_llm_and_missing_tts(monkeypatch) -> None:
    monkeypatch.setenv("NOISEBOT_LLM_PROVIDER", "lmstudio")
    monkeypatch.setenv("NOISEBOT_LLM_MODEL", "google/gemma-4-12b-qat")
    monkeypatch.setenv("NOISEBOT_LMSTUDIO_BASE_URL", "http://192.168.0.10:1234/v1")
    monkeypatch.setenv("NOISEBOT_TTS_PROVIDER", "piper")
    monkeypatch.setenv("NOISEBOT_PIPER_EXECUTABLE", "piper.exe")
    monkeypatch.setenv("NOISEBOT_PIPER_MODEL", "C:/missing/model.onnx")

    report = run_provider_doctor_from_env(active_axes=("llm", "tts"))

    assert report.llm.ok is True
    assert "lmstudio configurado" in report.llm.detail
    assert report.tts.ok is False
    assert "modelo Piper nao encontrado" in report.tts.detail


def test_provider_doctor_from_env_checks_stt_with_valid_wav(monkeypatch, tmp_path: Path) -> None:
    wav_path = tmp_path / "sample.wav"
    _write_test_wav(wav_path, b"\x01\x02\x03\x04")
    monkeypatch.setenv("NOISEBOT_STT_PROVIDER", "faster_whisper")
    monkeypatch.setattr("noisebot2.provider_smoke.importlib.util.find_spec", lambda name: object() if name == "faster_whisper" else None)

    report = run_provider_doctor_from_env(active_axes=("stt",), stt_wav_path=str(wav_path))

    assert report.ok is True
    assert "wav valido" in report.stt.detail


def test_provider_smoke_main_returns_nonzero_on_failure(monkeypatch, capsys) -> None:
    async def fake_run_provider_smoke_from_env(
        *,
        dotenv_path=None,
        prompt="",
        active_axes=(),
        tts_text="",
        stt_wav_path=None,
    ) -> ProviderSmokeReport:
        del dotenv_path, prompt
        assert active_axes == ("llm", "tts", "stt")
        assert tts_text == "teste de voz"
        assert stt_wav_path is None
        return ProviderSmokeReport(
            llm=ProviderSmokeResult("llm", False, "offline"),
            tts=ProviderSmokeResult("tts", False, "offline"),
            stt=ProviderSmokeResult("stt", False, "offline"),
        )

    monkeypatch.setattr("noisebot2.provider_smoke.run_provider_smoke_from_env", fake_run_provider_smoke_from_env)

    code = main(["--json"])

    assert code == 1
    assert capsys.readouterr().out.strip() == (
        '{"ok": false, "active_axes": ["llm", "tts", "stt"], "llm": {"provider": "llm", "ok": false, "detail": "offline"}, '
        '"tts": {"provider": "tts", "ok": false, "detail": "offline"}, '
        '"stt": {"provider": "stt", "ok": false, "detail": "offline"}}'
    )


def test_provider_smoke_main_returns_zero_for_llm_only_success(monkeypatch, capsys) -> None:
    async def fake_run_provider_smoke_from_env(
        *,
        dotenv_path=None,
        prompt="",
        active_axes=(),
        tts_text="",
        stt_wav_path=None,
    ) -> ProviderSmokeReport:
        del dotenv_path, prompt
        assert active_axes == ("llm",)
        assert tts_text == "teste de voz"
        assert stt_wav_path is None
        return ProviderSmokeReport(
            llm=ProviderSmokeResult("llm", True, "Oi, como posso ajudar?"),
            tts=ProviderSmokeResult("tts", False, "skipped"),
            stt=ProviderSmokeResult("stt", False, "skipped"),
            active_axes=("llm",),
        )

    monkeypatch.setattr("noisebot2.provider_smoke.run_provider_smoke_from_env", fake_run_provider_smoke_from_env)

    code = main(["--json", "--only", "llm"])

    assert code == 0
    assert capsys.readouterr().out.strip() == (
        '{"ok": true, "active_axes": ["llm"], "llm": {"provider": "llm", "ok": true, "detail": "Oi, como posso ajudar?"}, '
        '"tts": {"provider": "tts", "ok": false, "detail": "skipped"}, '
        '"stt": {"provider": "stt", "ok": false, "detail": "skipped"}}'
    )


def test_provider_smoke_main_passes_text_and_wav_arguments(monkeypatch, tmp_path: Path, capsys) -> None:
    wav_path = tmp_path / "sample.wav"
    _write_test_wav(wav_path, b"\x01\x02")

    async def fake_run_provider_smoke_from_env(
        *,
        dotenv_path=None,
        prompt="",
        active_axes=(),
        tts_text="",
        stt_wav_path=None,
    ) -> ProviderSmokeReport:
        del dotenv_path, prompt
        assert active_axes == ("stt",)
        assert tts_text == "fala alvo"
        assert stt_wav_path == str(wav_path)
        return ProviderSmokeReport(
            llm=ProviderSmokeResult("llm", False, "skipped"),
            tts=ProviderSmokeResult("tts", False, "skipped"),
            stt=ProviderSmokeResult("stt", True, "pt:teste"),
            active_axes=("stt",),
        )

    monkeypatch.setattr("noisebot2.provider_smoke.run_provider_smoke_from_env", fake_run_provider_smoke_from_env)

    code = main(["--json", "--only", "stt", "--text", "fala alvo", "--wav", str(wav_path)])

    assert code == 0
    assert capsys.readouterr().out.strip() == (
        '{"ok": true, "active_axes": ["stt"], "llm": {"provider": "llm", "ok": false, "detail": "skipped"}, '
        '"tts": {"provider": "tts", "ok": false, "detail": "skipped"}, '
        '"stt": {"provider": "stt", "ok": true, "detail": "pt:teste"}}'
    )


def test_provider_doctor_main_returns_nonzero_on_missing_tts(monkeypatch, capsys) -> None:
    def fake_doctor(*, dotenv_path=None, active_axes=(), stt_wav_path=None) -> ProviderDoctorReport:
        del dotenv_path, stt_wav_path
        assert active_axes == ("tts",)
        return ProviderDoctorReport(
            llm=ProviderSmokeResult("llm", False, "skipped"),
            tts=ProviderSmokeResult("tts", False, "modelo Piper nao encontrado: C:/missing.onnx"),
            stt=ProviderSmokeResult("stt", False, "skipped"),
            active_axes=("tts",),
        )

    monkeypatch.setattr("noisebot2.provider_smoke.run_provider_doctor_from_env", fake_doctor)

    code = main(["--json", "--doctor", "--only", "tts"])

    assert code == 1
    assert capsys.readouterr().out.strip() == (
        '{"ok": false, "active_axes": ["tts"], "llm": {"provider": "llm", "ok": false, "detail": "skipped"}, '
        '"tts": {"provider": "tts", "ok": false, "detail": "modelo Piper nao encontrado: C:/missing.onnx"}, '
        '"stt": {"provider": "stt", "ok": false, "detail": "skipped"}}'
    )
