"""Composição mínima da mente para S4.4."""

from __future__ import annotations

from noisebot2.bus import EventBus
from noisebot2.config import (
    load_dotenv,
    load_llm_config,
    load_nbp2_transport_config,
    load_stt_config,
    load_tts_config,
)
from noisebot2.mind.local_intents import LocalIntentResolver
from noisebot2.mind.mind_output import MindOutput, TtsProvider
from noisebot2.mind.turn_engine import IntentResolver, LlmProvider, TurnEngine
from noisebot2.providers import build_llm_provider, build_stt_provider, build_tts_provider
from noisebot2.providers.llm import AbstractLlmProvider
from noisebot2.providers.stt import AbstractSttProvider
from noisebot2.providers.tts import AbstractTtsProvider
from noisebot2.transport import Nbp2TransportServer


class MindRuntime:
    """Sobe os atores S4.4 sobre um bus comum."""

    def __init__(
        self,
        *,
        bus: EventBus | None = None,
        intent_resolver: IntentResolver | None = None,
        llm_provider: LlmProvider | AbstractLlmProvider | None = None,
        stt_provider: AbstractSttProvider | None = None,
        tts_provider: TtsProvider | AbstractTtsProvider | None = None,
        transport: Nbp2TransportServer | None = None,
    ) -> None:
        self.bus = bus or EventBus()
        self.llm_provider = llm_provider
        self.stt_provider = stt_provider
        self.tts_provider = tts_provider
        self.transport = transport
        self.intent_resolver = intent_resolver or LocalIntentResolver(event_publisher=self.bus.publish_nowait)
        self.turn_engine = TurnEngine(
            self.bus,
            intent_resolver=self.intent_resolver,
            llm_provider=llm_provider,
        )
        self.mind_output = MindOutput(self.bus, tts_provider=tts_provider)

    async def start(self) -> None:
        if self.transport is not None:
            await self.transport.start()
        await self.turn_engine.start()
        await self.mind_output.start()

    async def shutdown(self) -> None:
        await self.turn_engine.shutdown()
        await self.mind_output.shutdown()
        if self.transport is not None:
            await self.transport.shutdown()
        await self._close_provider(self.llm_provider)
        await self._close_provider(self.stt_provider)
        await self._close_provider(self.tts_provider)

    @classmethod
    def from_env(
        cls,
        *,
        bus: EventBus | None = None,
        dotenv_path: str | None = None,
        intent_resolver: IntentResolver | None = None,
        stt_provider: AbstractSttProvider | None = None,
        tts_provider: TtsProvider | AbstractTtsProvider | None = None,
    ) -> "MindRuntime":
        load_dotenv(dotenv_path)
        resolved_bus = bus or EventBus()
        llm_config = load_llm_config()
        transport_config = load_nbp2_transport_config()
        stt_config = load_stt_config()
        tts_config = load_tts_config()
        llm_provider = build_llm_provider(llm_config)
        resolved_stt_provider = stt_provider or build_stt_provider(stt_config)
        resolved_tts_provider = tts_provider or build_tts_provider(tts_config)
        transport = Nbp2TransportServer(resolved_bus, config=transport_config)
        resolved_intent_resolver = intent_resolver or LocalIntentResolver(
            event_publisher=resolved_bus.publish_nowait,
            status_provider=lambda: _build_status_reply(
                llm_config=llm_config,
                stt_config=stt_config,
                tts_config=tts_config,
            )
        )
        return cls(
            bus=resolved_bus,
            intent_resolver=resolved_intent_resolver,
            llm_provider=llm_provider,
            stt_provider=resolved_stt_provider,
            tts_provider=resolved_tts_provider,
            transport=transport,
        )

    async def _close_provider(self, provider: object | None) -> None:
        if provider is None:
            return
        close = getattr(provider, "close", None)
        if close is None:
            return
        await close()


def _build_status_reply(*, llm_config, stt_config, tts_config) -> str:
    llm_label = llm_config.provider.value
    stt_label = stt_config.provider.value
    tts_label = tts_config.provider.value
    return (
        "Estou operacional. "
        f"LLM: {llm_label}. "
        f"STT: {stt_label}. "
        f"TTS: {tts_label}."
    )
