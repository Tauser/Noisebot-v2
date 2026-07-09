"""Providers TTS do NoiseBot 2."""

from __future__ import annotations

import asyncio
from abc import ABC, abstractmethod
from collections.abc import AsyncIterator
from pathlib import Path

from .circuit_breaker import CircuitBreaker


class TtsProviderError(RuntimeError):
    """Falha operacional de provider TTS."""


class AbstractTtsProvider(ABC):
    @abstractmethod
    async def synthesize(self, text: str) -> AsyncIterator[bytes]:
        ...

    async def close(self) -> None:
        """Libera recursos opcionais do provider."""
        return None


class StaticTtsProvider(AbstractTtsProvider):
    def __init__(self, *chunks: bytes) -> None:
        self._chunks = chunks or (b"",)

    async def synthesize(self, text: str) -> AsyncIterator[bytes]:
        del text
        for chunk in self._chunks:
            yield chunk


class PiperTtsProvider(AbstractTtsProvider):
    def __init__(
        self,
        *,
        executable: str = "piper",
        model: str,
        use_cache: bool = True,
        circuit_breaker: CircuitBreaker | None = None,
    ) -> None:
        self._executable = executable
        self._model = model
        self._use_cache = use_cache
        self._cache: dict[str, bytes] = {}
        self._breaker = circuit_breaker or CircuitBreaker(provider=f"piper/{Path(model).name}")

    async def synthesize(self, text: str) -> AsyncIterator[bytes]:
        if not text.strip():
            return
        cached = self._cache.get(text)
        if cached is not None:
            yield cached
            return
        self._breaker.allow_request()
        if not Path(self._model).exists():
            raise TtsProviderError(f"modelo Piper nao encontrado: {self._model}")
        proc = await asyncio.create_subprocess_exec(
            self._executable,
            "--model",
            self._model,
            "--output-raw",
            stdin=asyncio.subprocess.PIPE,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
        )
        assert proc.stdin is not None
        assert proc.stdout is not None
        proc.stdin.write(text.encode("utf-8"))
        await proc.stdin.drain()
        proc.stdin.close()
        audio = bytearray()
        while True:
            chunk = await proc.stdout.read(512)
            if not chunk:
                break
            audio.extend(chunk)
        stderr = await proc.stderr.read() if proc.stderr is not None else b""
        code = await proc.wait()
        if code != 0:
            self._breaker.record_failure()
            raise TtsProviderError(f"piper saiu com codigo {code}: {stderr[:200].decode(errors='ignore')}")
        rendered = bytes(audio)
        if self._use_cache:
            self._cache[text] = rendered
        self._breaker.record_success()
        if rendered:
            yield rendered

    async def close(self) -> None:
        self._cache.clear()
