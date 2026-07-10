"""Atores da mente. Nenhum ator importa outro ator — só o bus e os contratos.

Especificação completa: docs/SERVER.md (FSM do turno, invariantes I-1..I-5,
providers, tools, stores). Nesta fase S4.4 entram os dois primeiros atores
reais: `TurnEngine` e `MindOutput`.
"""

from .contracts import (
    BargeInDetected,
    FinalTranscript,
    IntentResolved,
    QuietModeSetRequested,
    ReplyReady,
    SayAudio,
    SayBegin,
    SayCancel,
    SayEnd,
    SentenceReady,
    SessionStarted,
    SpeechCancellationRequested,
    SpeechDone,
    TimerSetRequested,
    TurnBudgetReported,
    VolumeSetRequested,
    TurnCancelled,
    TurnError,
    TurnManager,
    TurnState,
    TurnStateChanged,
    WakeDetected,
)
from .mind_output import MindOutput
from .turn_engine import TurnEngine

__all__ = [
    "BargeInDetected",
    "FinalTranscript",
    "IntentResolved",
    "MindOutput",
    "QuietModeSetRequested",
    "ReplyReady",
    "SayAudio",
    "SayBegin",
    "SayCancel",
    "SayEnd",
    "SentenceReady",
    "SessionStarted",
    "SpeechCancellationRequested",
    "SpeechDone",
    "TimerSetRequested",
    "TurnBudgetReported",
    "VolumeSetRequested",
    "TurnCancelled",
    "TurnEngine",
    "TurnError",
    "TurnManager",
    "TurnState",
    "TurnStateChanged",
    "WakeDetected",
]
