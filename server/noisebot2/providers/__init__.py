"""Providers de STT, LLM e TTS do NoiseBot 2."""

from .circuit_breaker import CircuitBreaker, CircuitOpenError, CircuitState
from .factory import build_llm_provider, build_stt_provider, build_tts_provider
from .llm import (
    AbstractLlmProvider,
    AnthropicLlmProvider,
    ChatCompletionsLlmProvider,
    LlmProviderError,
    OllamaLlmProvider,
    StaticLlmProvider,
)
from .stt import AbstractSttProvider, FasterWhisperSttProvider, SttProviderError, SttTranscript
from .tts import AbstractTtsProvider, PiperTtsProvider, StaticTtsProvider, TtsProviderError

__all__ = [
    "AbstractLlmProvider",
    "AbstractSttProvider",
    "AbstractTtsProvider",
    "AnthropicLlmProvider",
    "build_llm_provider",
    "build_stt_provider",
    "build_tts_provider",
    "ChatCompletionsLlmProvider",
    "CircuitBreaker",
    "CircuitOpenError",
    "CircuitState",
    "FasterWhisperSttProvider",
    "LlmProviderError",
    "OllamaLlmProvider",
    "PiperTtsProvider",
    "StaticLlmProvider",
    "StaticTtsProvider",
    "SttProviderError",
    "SttTranscript",
    "TtsProviderError",
]
