"""Providers LLM do NoiseBot 2."""

from __future__ import annotations

from abc import ABC, abstractmethod
from typing import Any

import aiohttp

from .circuit_breaker import CircuitBreaker

DEFAULT_FALLBACK_REPLY = "Nao consegui pensar agora, mas continuo aqui com voce."


class LlmProviderError(RuntimeError):
    """Falha operacional de provider LLM."""


class AbstractLlmProvider(ABC):
    @abstractmethod
    async def generate_reply(self, text: str, context: dict[str, Any]) -> str:
        ...


class StaticLlmProvider(AbstractLlmProvider):
    def __init__(self, reply: str) -> None:
        self._reply = reply

    async def generate_reply(self, text: str, context: dict[str, Any]) -> str:
        del text, context
        return self._reply


class ChatCompletionsLlmProvider(AbstractLlmProvider):
    def __init__(
        self,
        *,
        provider_label: str,
        model: str,
        api_key: str,
        base_url: str = "https://api.openai.com/v1",
        timeout_s: float = 10.0,
        max_output_tokens: int = 384,
        circuit_breaker: CircuitBreaker | None = None,
        session_factory: Any | None = None,
    ) -> None:
        self._provider_label = provider_label
        self._model = model
        self._api_key = api_key
        self._base_url = base_url.rstrip("/")
        self._timeout_s = timeout_s
        self._max_output_tokens = max_output_tokens
        self._session_factory = session_factory or aiohttp.ClientSession
        self._breaker = circuit_breaker or CircuitBreaker(provider=f"{provider_label}/{model}")

    async def generate_reply(self, text: str, context: dict[str, Any]) -> str:
        self._breaker.allow_request()
        payload = {
            "model": self._model,
            "messages": [
                {"role": "system", "content": "Responda em portugues do Brasil, de forma natural."},
                {"role": "user", "content": text},
            ],
            "temperature": 0.7,
        }
        payload["max_tokens"] = int(context.get("max_output_tokens", self._max_output_tokens))

        timeout = aiohttp.ClientTimeout(total=self._timeout_s)
        headers = {
            "Authorization": f"Bearer {self._api_key}",
            "Content-Type": "application/json",
        }
        try:
            async with self._session_factory(timeout=timeout) as session:
                async with session.post(
                    f"{self._base_url}/chat/completions",
                    headers=headers,
                    json=payload,
                ) as response:
                    if response.status >= 400:
                        body = await response.text()
                        raise LlmProviderError(
                            f"{self._provider_label} status={response.status}: {body[:200]}"
                        )
                    data = await response.json()
            content = (
                data.get("choices", [{}])[0]
                .get("message", {})
                .get("content", "")
            )
            if not isinstance(content, str) or not content.strip():
                raise LlmProviderError(f"{self._provider_label} retornou resposta vazia")
            self._breaker.record_success()
            return content.strip()
        except Exception as exc:
            self._breaker.record_failure()
            if isinstance(exc, LlmProviderError):
                raise
            raise LlmProviderError(str(exc)) from exc


class OllamaLlmProvider(AbstractLlmProvider):
    def __init__(
        self,
        *,
        model: str,
        base_url: str = "http://127.0.0.1:11434",
        timeout_s: float = 10.0,
        num_ctx: int = 16384,
        circuit_breaker: CircuitBreaker | None = None,
    ) -> None:
        self._model = model
        self._base_url = base_url.rstrip("/")
        self._timeout_s = timeout_s
        self._num_ctx = num_ctx
        self._breaker = circuit_breaker or CircuitBreaker(provider=f"ollama/{model}")

    async def generate_reply(self, text: str, context: dict[str, Any]) -> str:
        self._breaker.allow_request()
        payload = {
            "model": self._model,
            "messages": [
                {"role": "system", "content": "Responda em portugues do Brasil, de forma natural."},
                {"role": "user", "content": text},
            ],
            "stream": False,
            "options": {"num_ctx": int(context.get("num_ctx", self._num_ctx))},
        }
        timeout = aiohttp.ClientTimeout(total=self._timeout_s)
        try:
            async with aiohttp.ClientSession(timeout=timeout) as session:
                async with session.post(
                    f"{self._base_url}/api/chat",
                    json=payload,
                ) as response:
                    if response.status >= 400:
                        body = await response.text()
                        raise LlmProviderError(f"ollama status={response.status}: {body[:200]}")
                    data = await response.json()
            content = data.get("message", {}).get("content", "")
            if not isinstance(content, str) or not content.strip():
                raise LlmProviderError("ollama retornou resposta vazia")
            self._breaker.record_success()
            return content.strip()
        except Exception as exc:
            self._breaker.record_failure()
            if isinstance(exc, LlmProviderError):
                raise
            raise LlmProviderError(str(exc)) from exc


class AnthropicLlmProvider(AbstractLlmProvider):
    def __init__(
        self,
        *,
        model: str,
        api_key: str,
        base_url: str = "https://api.anthropic.com/v1",
        timeout_s: float = 10.0,
        max_output_tokens: int = 384,
        circuit_breaker: CircuitBreaker | None = None,
        session_factory: Any | None = None,
    ) -> None:
        self._model = model
        self._api_key = api_key
        self._base_url = base_url.rstrip("/")
        self._timeout_s = timeout_s
        self._max_output_tokens = max_output_tokens
        self._session_factory = session_factory or aiohttp.ClientSession
        self._breaker = circuit_breaker or CircuitBreaker(provider=f"anthropic/{model}")

    async def generate_reply(self, text: str, context: dict[str, Any]) -> str:
        self._breaker.allow_request()
        payload = {
            "model": self._model,
            "max_tokens": int(context.get("max_output_tokens", self._max_output_tokens)),
            "system": "Responda em portugues do Brasil, de forma natural.",
            "messages": [{"role": "user", "content": text}],
        }
        timeout = aiohttp.ClientTimeout(total=self._timeout_s)
        headers = {
            "x-api-key": self._api_key,
            "anthropic-version": "2023-06-01",
            "content-type": "application/json",
        }
        try:
            async with self._session_factory(timeout=timeout) as session:
                async with session.post(
                    f"{self._base_url}/messages",
                    headers=headers,
                    json=payload,
                ) as response:
                    if response.status >= 400:
                        body = await response.text()
                        raise LlmProviderError(f"anthropic status={response.status}: {body[:200]}")
                    data = await response.json()
            content_list = data.get("content", [])
            text_parts = [
                item.get("text", "")
                for item in content_list
                if isinstance(item, dict) and item.get("type") == "text"
            ]
            content = "".join(text_parts).strip()
            if not content:
                raise LlmProviderError("anthropic retornou resposta vazia")
            self._breaker.record_success()
            return content
        except Exception as exc:
            self._breaker.record_failure()
            if isinstance(exc, LlmProviderError):
                raise
            raise LlmProviderError(str(exc)) from exc
