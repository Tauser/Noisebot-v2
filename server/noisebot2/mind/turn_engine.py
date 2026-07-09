"""Ator dono da FSM do turno de voz."""

from __future__ import annotations

import asyncio
import inspect
import logging
from collections.abc import Awaitable, Callable
from typing import Any

from noisebot2.bus import EventBus

from .contracts import (
    BargeInDetected,
    FinalTranscript,
    IntentResolved,
    ReplyReady,
    SessionContext,
    SessionStarted,
    SpeechCancellationRequested,
    SpeechDone,
    TurnCancelled,
    TurnError,
    TurnManager,
    TurnState,
    TurnStateChanged,
    WakeDetected,
)

log = logging.getLogger(__name__)

IntentResolver = Callable[[str, SessionContext], IntentResolved | None | Awaitable[IntentResolved | None]]
LlmProvider = Callable[[str, SessionContext], str | Awaitable[str]]

FALLBACK_REPLY = "Nao consegui pensar agora, mas continuo aqui com voce."


class TurnEngine:
    """FSM do turno de voz e coordenação de cancelamentos."""

    def __init__(
        self,
        bus: EventBus,
        *,
        intent_resolver: IntentResolver | None = None,
        llm_provider: LlmProvider | None = None,
        fallback_reply: str = FALLBACK_REPLY,
    ) -> None:
        self._bus = bus
        self._intent_resolver = intent_resolver
        self._llm_provider = llm_provider
        self._fallback_reply = fallback_reply
        self._fsm = TurnManager()
        self._session: SessionContext | None = None
        self._speech_done_waiter: asyncio.Future[None] | None = None
        self._turn_task: asyncio.Task[None] | None = None
        self._events = bus.subscribe(WakeDetected, FinalTranscript, BargeInDetected, SpeechDone)
        self._runner: asyncio.Task[None] | None = None

    @property
    def state(self) -> TurnState:
        return self._fsm.state

    @property
    def current_turn_id(self) -> int:
        return self._fsm.current_turn_id

    @property
    def active_turn_task(self) -> asyncio.Task[None] | None:
        return self._turn_task

    async def start(self) -> None:
        if self._runner is None:
            self._runner = asyncio.create_task(self.run())

    async def shutdown(self) -> None:
        await self._cancel_turn_task()
        if self._runner is not None:
            self._runner.cancel()
            try:
                await self._runner
            except asyncio.CancelledError:
                pass
            self._runner = None

    async def run(self) -> None:
        async for event in self._bus.iter_queue(self._events):
            if isinstance(event, WakeDetected):
                await self._on_wake(event)
            elif isinstance(event, FinalTranscript):
                await self._on_final_transcript(event)
            elif isinstance(event, BargeInDetected):
                await self._on_barge_in(event)
            elif isinstance(event, SpeechDone):
                await self._on_speech_done(event)

    async def _on_wake(self, _event: WakeDetected) -> None:
        if self.state in (TurnState.THINKING, TurnState.SPEAKING) and self._session is not None:
            await self._bus.publish(BargeInDetected(turn_id=self._session.turn_id))
            return
        await self._replace_with_new_turn("wake")

    async def _on_final_transcript(self, event: FinalTranscript) -> None:
        if self._session is None:
            return
        if event.turn_id != self._session.turn_id:
            return
        if self.state != TurnState.LISTENING:
            return

        self._session.final_text = event.text.strip()
        await self._transition(TurnState.COMMITTING)
        self._turn_task = asyncio.create_task(self._run_turn(self._session))

    async def _on_barge_in(self, event: BargeInDetected) -> None:
        if self._session is None or event.turn_id != self._session.turn_id:
            return
        await self._interrupt_current_turn("barge_in")
        await self._replace_with_new_turn("barge_in")

    async def _on_speech_done(self, event: SpeechDone) -> None:
        if self._session is None or event.turn_id != self._session.turn_id:
            return
        if self._speech_done_waiter is not None and not self._speech_done_waiter.done():
            self._speech_done_waiter.set_result(None)

    async def _replace_with_new_turn(self, reason: str) -> None:
        previous_state = self.state
        if previous_state != TurnState.IDLE:
            try:
                self._fsm.transition(TurnState.INTERRUPTED)
                await self._bus.publish(
                    TurnStateChanged(
                        turn_id=self.current_turn_id,
                        previous_state=previous_state,
                        new_state=TurnState.INTERRUPTED,
                    )
                )
            except ValueError:
                self._fsm.reset_to_idle()

        session = SessionContext()
        self._session = session
        self._fsm.reset_to_idle()
        await self._transition(TurnState.LISTENING, turn_id=session.turn_id)
        await self._bus.publish(SessionStarted(turn_id=session.turn_id, reason=reason))

    async def _interrupt_current_turn(self, reason: str) -> None:
        if self._session is None:
            return
        turn_id = self._session.turn_id
        await self._bus.publish(SpeechCancellationRequested(turn_id=turn_id, reason=reason))
        await self._bus.publish(TurnCancelled(turn_id=turn_id, reason=reason))
        await self._cancel_turn_task()
        self._speech_done_waiter = None

    async def _cancel_turn_task(self) -> None:
        if self._turn_task is None:
            return
        if not self._turn_task.done():
            self._turn_task.cancel()
            try:
                await self._turn_task
            except asyncio.CancelledError:
                pass
        self._turn_task = None

    async def _run_turn(self, session: SessionContext) -> None:
        try:
            await self._transition(TurnState.THINKING)
            intent = await self._resolve_intent(session.final_text, session)
            if intent is not None and intent.has_intent and intent.reply_text:
                session.reply_text = intent.reply_text
                await self._bus.publish(intent)
                reply = ReplyReady(turn_id=session.turn_id, text=intent.reply_text, source="intent")
            else:
                reply_text = await self._generate_reply(session.final_text, session)
                session.reply_text = reply_text
                reply = ReplyReady(turn_id=session.turn_id, text=reply_text, source="llm")

            await self._transition(TurnState.SPEAKING)
            self._speech_done_waiter = asyncio.get_running_loop().create_future()
            await self._bus.publish(reply)
            await self._speech_done_waiter
            if self._session is session:
                await self._transition(TurnState.IDLE)
                self._session = None
        except asyncio.CancelledError:
            raise
        except Exception as exc:
            log.exception("turn_engine: erro no turno %d", session.turn_id)
            await self._bus.publish(TurnError(turn_id=session.turn_id, stage="turn", reason=str(exc)))
            session.reply_text = self._fallback_reply
            self._speech_done_waiter = asyncio.get_running_loop().create_future()
            await self._transition(TurnState.SPEAKING)
            await self._bus.publish(
                ReplyReady(turn_id=session.turn_id, text=self._fallback_reply, source="fallback")
            )
            await self._speech_done_waiter
            if self._session is session:
                await self._transition(TurnState.IDLE)
                self._session = None
        finally:
            if self._turn_task is not None and self._turn_task.done():
                self._turn_task = None

    async def _resolve_intent(
        self, text: str, session: SessionContext
    ) -> IntentResolved | None:
        if self._intent_resolver is None:
            return None
        result = self._intent_resolver(text, session)
        if inspect.isawaitable(result):
            return await result
        return result

    async def _generate_reply(self, text: str, session: SessionContext) -> str:
        if self._llm_provider is None:
            return self._fallback_reply
        result = self._llm_provider(text, session)
        if inspect.isawaitable(result):
            return await result
        return result

    async def _transition(self, new_state: TurnState, turn_id: int | None = None) -> None:
        previous_state = self.state
        self._fsm.transition(new_state, turn_id=turn_id)
        await self._bus.publish(
            TurnStateChanged(
                turn_id=self.current_turn_id,
                previous_state=previous_state,
                new_state=new_state,
            )
        )
