"""Composição mínima da mente para S4.4."""

from __future__ import annotations

from noisebot2.bus import EventBus
from noisebot2.mind.mind_output import MindOutput, TtsProvider
from noisebot2.mind.turn_engine import IntentResolver, LlmProvider, TurnEngine


class MindRuntime:
    """Sobe os atores S4.4 sobre um bus comum."""

    def __init__(
        self,
        *,
        bus: EventBus | None = None,
        intent_resolver: IntentResolver | None = None,
        llm_provider: LlmProvider | None = None,
        tts_provider: TtsProvider | None = None,
    ) -> None:
        self.bus = bus or EventBus()
        self.turn_engine = TurnEngine(
            self.bus,
            intent_resolver=intent_resolver,
            llm_provider=llm_provider,
        )
        self.mind_output = MindOutput(self.bus, tts_provider=tts_provider)

    async def start(self) -> None:
        await self.turn_engine.start()
        await self.mind_output.start()

    async def shutdown(self) -> None:
        await self.turn_engine.shutdown()
        await self.mind_output.shutdown()
