"""Configuracao do server NoiseBot 2 via ambiente."""

from __future__ import annotations

import os
from dataclasses import dataclass
from enum import Enum
from pathlib import Path


class LlmProviderKind(str, Enum):
    NONE = "none"
    OLLAMA = "ollama"
    LMSTUDIO = "lmstudio"
    OPENAI = "openai"
    API_CHAT = "api_chat"
    OPENAI_COMPATIBLE = "openai_compatible"
    ANTHROPIC = "anthropic"


class TtsProviderKind(str, Enum):
    NONE = "none"
    PIPER = "piper"


class SttProviderKind(str, Enum):
    NONE = "none"
    FASTER_WHISPER = "faster_whisper"


@dataclass(frozen=True)
class Nbp2TransportConfig:
    bind_host: str
    bind_port: int
    robot_token: bytes

    def safe_dict(self) -> dict[str, object]:
        return {
            "bind_host": self.bind_host,
            "bind_port": self.bind_port,
            "robot_token_configured": bool(self.robot_token),
        }


@dataclass(frozen=True)
class LlmConfig:
    provider: LlmProviderKind
    model: str
    timeout_s: float
    max_output_tokens: int
    ollama_base_url: str
    lmstudio_base_url: str
    openai_base_url: str
    openai_api_key: str
    api_chat_base_url: str
    api_chat_api_key: str
    api_chat_label: str
    anthropic_base_url: str
    anthropic_api_key: str
    ollama_num_ctx: int

    def safe_dict(self) -> dict[str, object]:
        return {
            "provider": self.provider.value,
            "model": self.model,
            "timeout_s": self.timeout_s,
            "max_output_tokens": self.max_output_tokens,
            "ollama_base_url": self.ollama_base_url,
            "lmstudio_base_url": self.lmstudio_base_url,
            "openai_base_url": self.openai_base_url,
            "openai_api_key_configured": bool(self.openai_api_key),
            "api_chat_base_url": self.api_chat_base_url,
            "api_chat_api_key_configured": bool(self.api_chat_api_key),
            "api_chat_label": self.api_chat_label,
            "anthropic_base_url": self.anthropic_base_url,
            "anthropic_api_key_configured": bool(self.anthropic_api_key),
            "ollama_num_ctx": self.ollama_num_ctx,
        }


@dataclass(frozen=True)
class TtsConfig:
    provider: TtsProviderKind
    piper_executable: str
    piper_model: str

    def safe_dict(self) -> dict[str, object]:
        return {
            "provider": self.provider.value,
            "piper_executable": self.piper_executable,
            "piper_model_configured": bool(self.piper_model),
        }


@dataclass(frozen=True)
class SttConfig:
    provider: SttProviderKind
    faster_whisper_model: str
    faster_whisper_device: str
    faster_whisper_compute_type: str
    faster_whisper_cpu_threads: int
    faster_whisper_num_workers: int
    faster_whisper_language: str
    faster_whisper_beam_size: int
    timeout_s: float

    def safe_dict(self) -> dict[str, object]:
        return {
            "provider": self.provider.value,
            "faster_whisper_model": self.faster_whisper_model,
            "faster_whisper_device": self.faster_whisper_device,
            "faster_whisper_compute_type": self.faster_whisper_compute_type,
            "faster_whisper_cpu_threads": self.faster_whisper_cpu_threads,
            "faster_whisper_num_workers": self.faster_whisper_num_workers,
            "faster_whisper_language": self.faster_whisper_language,
            "faster_whisper_beam_size": self.faster_whisper_beam_size,
            "timeout_s": self.timeout_s,
        }


def _env(key: str, default: str = "") -> str:
    return os.environ.get(key, default).strip()


def _env_int(key: str, default: int) -> int:
    try:
        return int(_env(key, str(default)))
    except ValueError:
        return default


def _env_float(key: str, default: float) -> float:
    try:
        return float(_env(key, str(default)))
    except ValueError:
        return default


def load_dotenv(path: str | os.PathLike[str] | None = None) -> Path | None:
    candidates: list[Path] = []
    if path is not None:
        candidates.append(Path(path))
    else:
        here = Path(__file__).resolve()
        candidates.extend(
            [
                Path.cwd() / ".env",
                here.parents[1] / ".env",
                here.parents[2] / ".env",
            ]
        )

    for candidate in candidates:
        resolved = candidate.resolve()
        if not resolved.exists():
            continue
        for raw in resolved.read_text(encoding="utf-8").splitlines():
            line = raw.strip()
            if not line or line.startswith("#") or "=" not in line:
                continue
            key, _, value = line.partition("=")
            key = key.strip()
            value = value.strip().strip('"').strip("'")
            if key and not os.environ.get(key):
                os.environ[key] = value
        return resolved
    return None


def load_llm_config() -> LlmConfig:
    provider_raw = _env("NOISEBOT_LLM_PROVIDER", "ollama").lower()
    aliases = {
        "openai_compatible": LlmProviderKind.API_CHAT,
        "compatible": LlmProviderKind.API_CHAT,
        "generic_api": LlmProviderKind.API_CHAT,
        "api": LlmProviderKind.API_CHAT,
    }
    provider = aliases.get(provider_raw)
    if provider is None:
        try:
            provider = LlmProviderKind(provider_raw)
        except ValueError:
            provider = LlmProviderKind.OLLAMA

    default_models = {
        LlmProviderKind.NONE: "none",
        LlmProviderKind.OLLAMA: "gemma4:12b",
        LlmProviderKind.LMSTUDIO: "local-model",
        LlmProviderKind.OPENAI: "gpt-4o-mini",
        LlmProviderKind.API_CHAT: "compatible-model",
        LlmProviderKind.ANTHROPIC: "claude-3-5-sonnet-latest",
    }

    return LlmConfig(
        provider=provider,
        model=_env("NOISEBOT_LLM_MODEL", default_models[provider]),
        timeout_s=_env_float("NOISEBOT_LLM_TIMEOUT_S", 10.0),
        max_output_tokens=_env_int("NOISEBOT_LLM_MAX_OUTPUT_TOKENS", 384),
        ollama_base_url=_env("NOISEBOT_OLLAMA_BASE_URL", "http://127.0.0.1:11434"),
        lmstudio_base_url=_env("NOISEBOT_LMSTUDIO_BASE_URL", "http://127.0.0.1:1234/v1"),
        openai_base_url=_env("NOISEBOT_OPENAI_BASE_URL", "https://api.openai.com/v1"),
        openai_api_key=_env("OPENAI_API_KEY", ""),
        api_chat_base_url=_env(
            "NOISEBOT_API_CHAT_BASE_URL",
            _env("NOISEBOT_OPENAI_COMPATIBLE_BASE_URL", ""),
        ),
        api_chat_api_key=_env(
            "NOISEBOT_API_CHAT_API_KEY",
            _env("NOISEBOT_OPENAI_COMPATIBLE_API_KEY", ""),
        ),
        api_chat_label=_env("NOISEBOT_API_CHAT_LABEL", "custom_api"),
        anthropic_base_url=_env("NOISEBOT_ANTHROPIC_BASE_URL", "https://api.anthropic.com/v1"),
        anthropic_api_key=_env("ANTHROPIC_API_KEY", ""),
        ollama_num_ctx=max(4096, _env_int("NOISEBOT_OLLAMA_NUM_CTX", 16384)),
    )


def load_tts_config() -> TtsConfig:
    provider_raw = _env("NOISEBOT_TTS_PROVIDER", "none").lower()
    try:
        provider = TtsProviderKind(provider_raw)
    except ValueError:
        provider = TtsProviderKind.NONE

    return TtsConfig(
        provider=provider,
        piper_executable=_env("NOISEBOT_PIPER_EXECUTABLE", "piper"),
        piper_model=_env("NOISEBOT_PIPER_MODEL", ""),
    )


def load_stt_config() -> SttConfig:
    provider_raw = _env("NOISEBOT_STT_PROVIDER", "none").lower()
    aliases = {
        "whisper": SttProviderKind.FASTER_WHISPER,
        "faster-whisper": SttProviderKind.FASTER_WHISPER,
    }
    provider = aliases.get(provider_raw)
    if provider is None:
        try:
            provider = SttProviderKind(provider_raw)
        except ValueError:
            provider = SttProviderKind.NONE

    return SttConfig(
        provider=provider,
        faster_whisper_model=_env("NOISEBOT_FASTER_WHISPER_MODEL", "medium"),
        faster_whisper_device=_env("NOISEBOT_FASTER_WHISPER_DEVICE", "cpu"),
        faster_whisper_compute_type=_env("NOISEBOT_FASTER_WHISPER_COMPUTE_TYPE", "int8"),
        faster_whisper_cpu_threads=max(1, _env_int("NOISEBOT_FASTER_WHISPER_CPU_THREADS", 4)),
        faster_whisper_num_workers=max(1, _env_int("NOISEBOT_FASTER_WHISPER_NUM_WORKERS", 1)),
        faster_whisper_language=_env("NOISEBOT_FASTER_WHISPER_LANGUAGE", "pt"),
        faster_whisper_beam_size=max(1, _env_int("NOISEBOT_FASTER_WHISPER_BEAM_SIZE", 5)),
        timeout_s=max(1.0, _env_float("NOISEBOT_STT_TIMEOUT_S", 30.0)),
    )


def load_nbp2_transport_config() -> Nbp2TransportConfig:
    token_hex = _env("NOISEBOT_NBP2_TOKEN_HEX", "")
    token = b""
    if token_hex:
        try:
            token = bytes.fromhex(token_hex)
        except ValueError:
            token = b""
    else:
        token_path = Path.home() / ".noisebot2" / "robot_token"
        if token_path.exists():
            raw = token_path.read_text(encoding="utf-8").strip()
            if raw:
                try:
                    token = bytes.fromhex(raw)
                except ValueError:
                    token = raw.encode("utf-8")[:32]

    return Nbp2TransportConfig(
        bind_host=_env("NOISEBOT_NBP2_BIND_HOST", "127.0.0.1"),
        bind_port=max(1, _env_int("NOISEBOT_NBP2_BIND_PORT", 9100)),
        robot_token=token[:32],
    )
