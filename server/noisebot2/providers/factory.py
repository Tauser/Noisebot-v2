"""Fabrica de providers do NoiseBot 2."""

from __future__ import annotations

from noisebot2.config import LlmConfig, LlmProviderKind

from .llm import (
    AbstractLlmProvider,
    AnthropicLlmProvider,
    ChatCompletionsLlmProvider,
    OllamaLlmProvider,
)


def build_llm_provider(config: LlmConfig) -> AbstractLlmProvider | None:
    if config.provider == LlmProviderKind.NONE:
        return None
    if config.provider == LlmProviderKind.OLLAMA:
        return OllamaLlmProvider(
            model=config.model,
            base_url=config.ollama_base_url,
            timeout_s=config.timeout_s,
            num_ctx=config.ollama_num_ctx,
        )
    if config.provider == LlmProviderKind.LMSTUDIO:
        return ChatCompletionsLlmProvider(
            provider_label="lmstudio",
            model=config.model,
            base_url=config.lmstudio_base_url,
            api_key="lm-studio",
            timeout_s=config.timeout_s,
            max_output_tokens=config.max_output_tokens,
        )
    if config.provider == LlmProviderKind.OPENAI:
        return ChatCompletionsLlmProvider(
            provider_label="openai",
            model=config.model,
            base_url=config.openai_base_url,
            api_key=config.openai_api_key,
            timeout_s=config.timeout_s,
            max_output_tokens=config.max_output_tokens,
        )
    if config.provider in (LlmProviderKind.API_CHAT, LlmProviderKind.OPENAI_COMPATIBLE):
        return ChatCompletionsLlmProvider(
            provider_label=config.api_chat_label,
            model=config.model,
            base_url=config.api_chat_base_url,
            api_key=config.api_chat_api_key or "compatible-key",
            timeout_s=config.timeout_s,
            max_output_tokens=config.max_output_tokens,
        )
    if config.provider == LlmProviderKind.ANTHROPIC:
        return AnthropicLlmProvider(
            model=config.model,
            api_key=config.anthropic_api_key,
            base_url=config.anthropic_base_url,
            timeout_s=config.timeout_s,
            max_output_tokens=config.max_output_tokens,
        )
    raise ValueError(f"provider LLM nao suportado: {config.provider.value}")
