from __future__ import annotations

from datetime import datetime

from noisebot2.mind import IntentResolved, QuietModeSetRequested, TimerSetRequested, VolumeSetRequested
from noisebot2.mind.contracts import SessionContext
from noisebot2.mind.local_intents import LocalIntentResolver


def _turn(text: str, resolver: LocalIntentResolver) -> IntentResolved | None:
    return resolver(text, SessionContext(turn_id=7))


def test_local_intent_resolver_answers_time_query() -> None:
    resolver = LocalIntentResolver(clock=lambda: datetime(2026, 7, 9, 14, 35))

    resolved = _turn("que horas sao agora?", resolver)

    assert resolved is not None
    assert resolved.intent_name == "local_time"
    assert resolved.reply_text == "Agora sao 14:35."


def test_local_intent_resolver_answers_date_query() -> None:
    resolver = LocalIntentResolver(clock=lambda: datetime(2026, 7, 9, 14, 35))

    resolved = _turn("qual e a data de hoje?", resolver)

    assert resolved is not None
    assert resolved.intent_name == "local_date"
    assert resolved.reply_text == "Hoje e quinta-feira, 9 de julho de 2026."


def test_local_intent_resolver_answers_status_query() -> None:
    resolver = LocalIntentResolver(status_provider=lambda: "Estou operacional.")

    resolved = _turn("me diga o status do sistema", resolver)

    assert resolved is not None
    assert resolved.intent_name == "local_status"
    assert resolved.reply_text == "Estou operacional."


def test_local_intent_resolver_handles_unavailable_timer_locally() -> None:
    published: list[object] = []
    resolver = LocalIntentResolver(
        clock=lambda: datetime(2026, 7, 9, 14, 35),
        event_publisher=published.append,
    )

    resolved = _turn("crie um timer de 5 minutos", resolver)

    assert resolved is not None
    assert resolved.intent_name == "local_timer_create"
    assert resolved.reply_text == "Timer de 5 minutos criado para 14:40:00."
    assert len(published) == 1
    assert isinstance(published[0], TimerSetRequested)
    assert published[0].timer_id == 7
    assert published[0].duration_ms == 300000


def test_local_intent_resolver_creates_absolute_alarm_locally() -> None:
    published: list[object] = []
    resolver = LocalIntentResolver(
        clock=lambda: datetime(2026, 7, 9, 14, 35),
        event_publisher=published.append,
    )

    resolved = _turn("crie um alarme para as 16:45", resolver)

    assert resolved is not None
    assert resolved.intent_name == "local_alarm_create"
    assert resolved.reply_text == "Alarme criado para 16:45."
    assert len(published) == 1
    assert isinstance(published[0], TimerSetRequested)
    assert published[0].timer_id == 7
    assert published[0].label == "alarme 16:45"
    assert published[0].duration_ms == 7_800_000


def test_local_intent_resolver_sets_volume_locally() -> None:
    published: list[object] = []
    resolver = LocalIntentResolver(event_publisher=published.append)

    resolved = _turn("volume 40", resolver)

    assert resolved is not None
    assert resolved.intent_name == "local_volume_set"
    assert resolved.reply_text == "Volume ajustado para 40%."
    assert len(published) == 1
    assert isinstance(published[0], VolumeSetRequested)
    assert published[0].volume_percent == 40


def test_local_intent_resolver_enables_quiet_mode_locally() -> None:
    published: list[object] = []
    resolver = LocalIntentResolver(event_publisher=published.append)

    resolved = _turn("ative modo silencioso", resolver)

    assert resolved is not None
    assert resolved.intent_name == "local_quiet_mode_enable"
    assert resolved.reply_text == "Modo silencioso ativado."
    assert len(published) == 1
    assert isinstance(published[0], QuietModeSetRequested)
    assert published[0].enabled is True
