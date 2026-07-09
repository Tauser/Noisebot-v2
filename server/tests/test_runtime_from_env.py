from __future__ import annotations

from noisebot2.providers import FasterWhisperSttProvider, OllamaLlmProvider, PiperTtsProvider
from noisebot2.runtime import MindRuntime


def test_runtime_from_env_builds_configured_providers(monkeypatch) -> None:
    monkeypatch.setenv("NOISEBOT_LLM_PROVIDER", "ollama")
    monkeypatch.setenv("NOISEBOT_STT_PROVIDER", "faster_whisper")
    monkeypatch.setenv("NOISEBOT_TTS_PROVIDER", "piper")
    monkeypatch.setenv("NOISEBOT_PIPER_MODEL", "C:/models/pt_BR.onnx")

    runtime = MindRuntime.from_env()

    assert isinstance(runtime.turn_engine._llm_provider, OllamaLlmProvider)
    assert isinstance(runtime.stt_provider, FasterWhisperSttProvider)
    assert isinstance(runtime.mind_output._tts_provider, PiperTtsProvider)
