"""Providers de STT, LLM e TTS do NoiseBot 2."""

from .circuit_breaker import CircuitBreaker, CircuitOpenError, CircuitState
from .factory import build_llm_provider
from .llm import (
    AbstractLlmProvider,
    AnthropicLlmProvider,
    ChatCompletionsLlmProvider,
    LlmProviderError,
    OllamaLlmProvider,
    StaticLlmProvider,
)
from .tts import AbstractTtsProvider, PiperTtsProvider, StaticTtsProvider, TtsProviderError

__all__ = [
    "AbstractLlmProvider",
    "AbstractTtsProvider",
    "AnthropicLlmProvider",
    "build_llm_provider",
    "ChatCompletionsLlmProvider",
    "CircuitBreaker",
    "CircuitOpenError",
    "CircuitState",
    "LlmProviderError",
    "OllamaLlmProvider",
    "PiperTtsProvider",
    "StaticLlmProvider",
    "StaticTtsProvider",
    "TtsProviderError",
]
