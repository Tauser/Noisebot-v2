"""Contratos tipados dos atores da mente."""

from __future__ import annotations

import time
from dataclasses import dataclass, field
from enum import Enum, auto
from typing import Any

_TURN_COUNTER = 0


def _now() -> float:
    return time.monotonic()


def new_turn_id() -> int:
    global _TURN_COUNTER
    _TURN_COUNTER += 1
    return _TURN_COUNTER


class TurnState(Enum):
    IDLE = auto()
    LISTENING = auto()
    COMMITTING = auto()
    THINKING = auto()
    SPEAKING = auto()
    INTERRUPTED = auto()
    ERROR_RECOVERY = auto()


_VALID_TRANSITIONS: dict[TurnState, frozenset[TurnState]] = {
    TurnState.IDLE: frozenset({TurnState.LISTENING}),
    TurnState.LISTENING: frozenset(
        {TurnState.LISTENING, TurnState.COMMITTING, TurnState.IDLE, TurnState.INTERRUPTED}
    ),
    TurnState.COMMITTING: frozenset({TurnState.THINKING, TurnState.IDLE, TurnState.ERROR_RECOVERY}),
    TurnState.THINKING: frozenset(
        {TurnState.SPEAKING, TurnState.IDLE, TurnState.INTERRUPTED, TurnState.ERROR_RECOVERY}
    ),
    TurnState.SPEAKING: frozenset(
        {TurnState.SPEAKING, TurnState.IDLE, TurnState.INTERRUPTED, TurnState.ERROR_RECOVERY}
    ),
    TurnState.INTERRUPTED: frozenset({TurnState.LISTENING, TurnState.IDLE}),
    TurnState.ERROR_RECOVERY: frozenset({TurnState.IDLE}),
}


class TurnManager:
    """FSM mínima herdada do v1 para o turno de voz."""

    def __init__(self) -> None:
        self._state = TurnState.IDLE
        self._current_turn_id = 0

    @property
    def state(self) -> TurnState:
        return self._state

    @property
    def current_turn_id(self) -> int:
        return self._current_turn_id

    @property
    def can_interrupt(self) -> bool:
        return self._state in (TurnState.THINKING, TurnState.SPEAKING)

    def transition(self, new_state: TurnState, turn_id: int | None = None) -> None:
        allowed = _VALID_TRANSITIONS.get(self._state, frozenset())
        if new_state not in allowed:
            raise ValueError(f"Transicao invalida: {self._state.name} -> {new_state.name}")
        if turn_id is not None:
            self._current_turn_id = turn_id
        self._state = new_state

    def reset_to_idle(self) -> None:
        self._state = TurnState.IDLE


@dataclass(frozen=True)
class SessionStarted:
    turn_id: int
    reason: str = "wake"
    t: float = field(default_factory=_now)


@dataclass(frozen=True)
class TurnStateChanged:
    turn_id: int
    previous_state: TurnState
    new_state: TurnState
    t: float = field(default_factory=_now)


@dataclass(frozen=True)
class WakeDetected:
    session_hint: int = 0
    t: float = field(default_factory=_now)


@dataclass(frozen=True)
class FinalTranscript:
    turn_id: int
    text: str
    t: float = field(default_factory=_now)


@dataclass(frozen=True)
class IntentResolved:
    turn_id: int
    intent_name: str | None
    reply_text: str | None = None
    resolution_reason: str | None = None
    t: float = field(default_factory=_now)

    @property
    def has_intent(self) -> bool:
        return self.intent_name is not None


@dataclass(frozen=True)
class ReplyReady:
    turn_id: int
    text: str
    source: str
    t: float = field(default_factory=_now)


@dataclass(frozen=True)
class SentenceReady:
    turn_id: int
    sentence: str
    index: int = 0
    t: float = field(default_factory=_now)


@dataclass(frozen=True)
class SayBegin:
    turn_id: int
    t: float = field(default_factory=_now)


@dataclass(frozen=True)
class SayAudio:
    turn_id: int
    pcm: bytes
    index: int = 0
    t: float = field(default_factory=_now)


@dataclass(frozen=True)
class SayEnd:
    turn_id: int
    t: float = field(default_factory=_now)


@dataclass(frozen=True)
class SayCancel:
    turn_id: int
    reason: str = "cancelled"
    t: float = field(default_factory=_now)


@dataclass(frozen=True)
class SpeechDone:
    turn_id: int
    t: float = field(default_factory=_now)


@dataclass(frozen=True)
class SpeechCancellationRequested:
    turn_id: int
    reason: str
    t: float = field(default_factory=_now)


@dataclass(frozen=True)
class BargeInDetected:
    turn_id: int
    t: float = field(default_factory=_now)


@dataclass(frozen=True)
class TurnCancelled:
    turn_id: int
    reason: str
    t: float = field(default_factory=_now)


@dataclass(frozen=True)
class TurnError:
    turn_id: int
    stage: str
    reason: str
    t: float = field(default_factory=_now)


@dataclass
class SessionContext:
    turn_id: int = field(default_factory=new_turn_id)
    started_at: float = field(default_factory=_now)
    final_text: str = ""
    reply_text: str = ""
    meta: dict[str, Any] = field(default_factory=dict)
