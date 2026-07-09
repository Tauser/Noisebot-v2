"""Providers STT do NoiseBot 2."""

from __future__ import annotations

import asyncio
import tempfile
import wave
from abc import ABC, abstractmethod
from dataclasses import dataclass
from pathlib import Path

from .circuit_breaker import CircuitBreaker


class SttProviderError(RuntimeError):
    """Falha operacional de provider STT."""


@dataclass(frozen=True)
class SttTranscript:
    text: str
    language: str


class AbstractSttProvider(ABC):
    @abstractmethod
    async def transcribe_pcm16le(self, pcm: bytes, *, sample_rate_hz: int = 16000) -> SttTranscript:
        ...

    async def close(self) -> None:
        """Libera recursos opcionais do provider."""
        return None


class FasterWhisperSttProvider(AbstractSttProvider):
    def __init__(
        self,
        *,
        model: str,
        device: str = "cpu",
        compute_type: str = "int8",
        cpu_threads: int = 4,
        num_workers: int = 1,
        language: str = "pt",
        beam_size: int = 5,
        timeout_s: float = 30.0,
        circuit_breaker: CircuitBreaker | None = None,
    ) -> None:
        self._model_name = model
        self._device = device
        self._compute_type = compute_type
        self._cpu_threads = cpu_threads
        self._num_workers = num_workers
        self._language = language
        self._beam_size = beam_size
        self._timeout_s = timeout_s
        self._breaker = circuit_breaker or CircuitBreaker(provider=f"faster_whisper/{model}")
        self._model = None

    async def transcribe_pcm16le(self, pcm: bytes, *, sample_rate_hz: int = 16000) -> SttTranscript:
        if not pcm:
            return SttTranscript(text="", language=self._language)
        self._breaker.allow_request()
        try:
            transcript = await asyncio.wait_for(
                asyncio.to_thread(self._transcribe_sync, pcm, sample_rate_hz),
                timeout=self._timeout_s,
            )
            self._breaker.record_success()
            return transcript
        except Exception as exc:
            self._breaker.record_failure()
            if isinstance(exc, SttProviderError):
                raise
            raise SttProviderError(str(exc)) from exc

    def _ensure_model(self):
        if self._model is not None:
            return self._model
        try:
            from faster_whisper import WhisperModel  # type: ignore[import-not-found]
        except ImportError as exc:
            raise SttProviderError(
                "faster-whisper nao instalado. Instale com: pip install -e .[stt]"
            ) from exc
        self._model = WhisperModel(
            self._model_name,
            device=self._device,
            compute_type=self._compute_type,
            cpu_threads=self._cpu_threads,
            num_workers=self._num_workers,
        )
        return self._model

    def _transcribe_sync(self, pcm: bytes, sample_rate_hz: int) -> SttTranscript:
        model = self._ensure_model()
        with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as handle:
            temp_path = Path(handle.name)
        try:
            with wave.open(str(temp_path), "wb") as wav_file:
                wav_file.setnchannels(1)
                wav_file.setsampwidth(2)
                wav_file.setframerate(sample_rate_hz)
                wav_file.writeframes(pcm)
            segments, info = model.transcribe(
                str(temp_path),
                language=self._language,
                beam_size=self._beam_size,
                vad_filter=True,
            )
            text = " ".join(segment.text.strip() for segment in segments if segment.text.strip()).strip()
            language = getattr(info, "language", self._language) or self._language
            return SttTranscript(text=text, language=language)
        finally:
            temp_path.unlink(missing_ok=True)
