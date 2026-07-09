from __future__ import annotations

from noisebot2.config import (
    LlmProviderKind,
    SttProviderKind,
    TtsProviderKind,
    load_llm_config,
    load_stt_config,
    load_tts_config,
)
from noisebot2.providers import (
    AnthropicLlmProvider,
    ChatCompletionsLlmProvider,
    FasterWhisperSttProvider,
    OllamaLlmProvider,
    PiperTtsProvider,
)
from noisebot2.providers.factory import build_llm_provider, build_stt_provider, build_tts_provider


def test_load_llm_config_reads_lmstudio_env(monkeypatch) -> None:
    monkeypatch.setenv("NOISEBOT_LLM_PROVIDER", "lmstudio")
    monkeypatch.setenv("NOISEBOT_LLM_MODEL", "qwen-local")
    monkeypatch.setenv("NOISEBOT_LMSTUDIO_BASE_URL", "http://127.0.0.1:1234/v1")

    config = load_llm_config()

    assert config.provider == LlmProviderKind.LMSTUDIO
    assert config.model == "qwen-local"
    assert config.lmstudio_base_url == "http://127.0.0.1:1234/v1"


def test_load_llm_config_accepts_generic_api_alias(monkeypatch) -> None:
    monkeypatch.setenv("NOISEBOT_LLM_PROVIDER", "api")
    monkeypatch.setenv("NOISEBOT_LLM_MODEL", "grok-test")
    monkeypatch.setenv("NOISEBOT_API_CHAT_BASE_URL", "https://example.invalid/v1")
    monkeypatch.setenv("NOISEBOT_API_CHAT_API_KEY", "api-test")
    monkeypatch.setenv("NOISEBOT_API_CHAT_LABEL", "xai")

    config = load_llm_config()

    assert config.provider == LlmProviderKind.API_CHAT
    assert config.model == "grok-test"
    assert config.api_chat_base_url == "https://example.invalid/v1"
    assert config.api_chat_api_key == "api-test"
    assert config.api_chat_label == "xai"


def test_build_llm_provider_supports_local_and_online_variants(monkeypatch) -> None:
    monkeypatch.setenv("NOISEBOT_LLM_PROVIDER", "ollama")
    ollama = build_llm_provider(load_llm_config())
    assert isinstance(ollama, OllamaLlmProvider)

    monkeypatch.setenv("NOISEBOT_LLM_PROVIDER", "lmstudio")
    lmstudio = build_llm_provider(load_llm_config())
    assert isinstance(lmstudio, ChatCompletionsLlmProvider)

    monkeypatch.setenv("NOISEBOT_LLM_PROVIDER", "openai")
    monkeypatch.setenv("OPENAI_API_KEY", "sk-test")
    openai_provider = build_llm_provider(load_llm_config())
    assert isinstance(openai_provider, ChatCompletionsLlmProvider)

    monkeypatch.setenv("NOISEBOT_LLM_PROVIDER", "api_chat")
    monkeypatch.setenv("NOISEBOT_API_CHAT_BASE_URL", "https://example.invalid/v1")
    monkeypatch.setenv("NOISEBOT_API_CHAT_API_KEY", "api-test")
    generic_api = build_llm_provider(load_llm_config())
    assert isinstance(generic_api, ChatCompletionsLlmProvider)

    monkeypatch.setenv("NOISEBOT_LLM_PROVIDER", "anthropic")
    monkeypatch.setenv("ANTHROPIC_API_KEY", "ak-test")
    anthropic = build_llm_provider(load_llm_config())
    assert isinstance(anthropic, AnthropicLlmProvider)


def test_load_tts_config_reads_piper_env(monkeypatch) -> None:
    monkeypatch.setenv("NOISEBOT_TTS_PROVIDER", "piper")
    monkeypatch.setenv("NOISEBOT_PIPER_EXECUTABLE", "C:/tools/piper.exe")
    monkeypatch.setenv("NOISEBOT_PIPER_MODEL", "C:/models/pt_BR.onnx")

    config = load_tts_config()

    assert config.provider == TtsProviderKind.PIPER
    assert config.piper_executable == "C:/tools/piper.exe"
    assert config.piper_model == "C:/models/pt_BR.onnx"


def test_build_tts_provider_supports_piper(monkeypatch) -> None:
    monkeypatch.setenv("NOISEBOT_TTS_PROVIDER", "piper")
    monkeypatch.setenv("NOISEBOT_PIPER_MODEL", "C:/models/pt_BR.onnx")

    provider = build_tts_provider(load_tts_config())

    assert isinstance(provider, PiperTtsProvider)


def test_load_stt_config_reads_faster_whisper_env(monkeypatch) -> None:
    monkeypatch.setenv("NOISEBOT_STT_PROVIDER", "whisper")
    monkeypatch.setenv("NOISEBOT_FASTER_WHISPER_MODEL", "large-v3")
    monkeypatch.setenv("NOISEBOT_FASTER_WHISPER_DEVICE", "cuda")
    monkeypatch.setenv("NOISEBOT_FASTER_WHISPER_COMPUTE_TYPE", "float16")

    config = load_stt_config()

    assert config.provider == SttProviderKind.FASTER_WHISPER
    assert config.faster_whisper_model == "large-v3"
    assert config.faster_whisper_device == "cuda"
    assert config.faster_whisper_compute_type == "float16"


def test_build_stt_provider_supports_faster_whisper(monkeypatch) -> None:
    monkeypatch.setenv("NOISEBOT_STT_PROVIDER", "faster_whisper")

    provider = build_stt_provider(load_stt_config())

    assert isinstance(provider, FasterWhisperSttProvider)
