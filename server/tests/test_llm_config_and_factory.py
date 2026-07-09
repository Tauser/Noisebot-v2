from __future__ import annotations

from noisebot2.config import LlmProviderKind, load_llm_config
from noisebot2.providers import AnthropicLlmProvider, ChatCompletionsLlmProvider, OllamaLlmProvider
from noisebot2.providers.factory import build_llm_provider


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
