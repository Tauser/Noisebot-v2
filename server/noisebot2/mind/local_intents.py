"""Resolvedor local de intents offline-first para o TurnEngine."""

from __future__ import annotations

import re
from dataclasses import dataclass
from datetime import datetime, timedelta
from typing import Callable

from .contracts import (
    IntentResolved,
    QuietModeSetRequested,
    SessionContext,
    TimerSetRequested,
    VolumeSetRequested,
)

StatusProvider = Callable[[], str]
Clock = Callable[[], datetime]
EventPublisher = Callable[[object], None]


def _default_clock() -> datetime:
    return datetime.now().astimezone()


def _default_status() -> str:
    return "Estou operacional no modo offline basico."


@dataclass
class LocalIntentResolver:
    """Aplica regras simples em PT-BR antes de qualquer LLM."""

    clock: Clock = _default_clock
    status_provider: StatusProvider = _default_status
    event_publisher: EventPublisher | None = None

    def __call__(self, text: str, session: SessionContext) -> IntentResolved | None:
        normalized = _normalize(text)
        if not normalized:
            return None

        if _matches_time_query(normalized):
            now = self.clock()
            return IntentResolved(
                turn_id=session.turn_id,
                intent_name="local_time",
                reply_text=f"Agora sao {now.strftime('%H:%M')}.",
                resolution_reason="offline_first",
            )

        if _matches_date_query(normalized):
            now = self.clock()
            weekdays = [
                "segunda-feira",
                "terca-feira",
                "quarta-feira",
                "quinta-feira",
                "sexta-feira",
                "sabado",
                "domingo",
            ]
            months = [
                "janeiro",
                "fevereiro",
                "marco",
                "abril",
                "maio",
                "junho",
                "julho",
                "agosto",
                "setembro",
                "outubro",
                "novembro",
                "dezembro",
            ]
            weekday = weekdays[now.weekday()]
            month = months[now.month - 1]
            return IntentResolved(
                turn_id=session.turn_id,
                intent_name="local_date",
                reply_text=f"Hoje e {weekday}, {now.day} de {month} de {now.year}.",
                resolution_reason="offline_first",
            )

        if _matches_status_query(normalized):
            return IntentResolved(
                turn_id=session.turn_id,
                intent_name="local_status",
                reply_text=self.status_provider(),
                resolution_reason="offline_first",
            )

        schedule_request = _parse_schedule_request(normalized, self.clock())
        if schedule_request is not None:
            timer_id = session.turn_id
            if self.event_publisher is not None:
                self.event_publisher(
                    TimerSetRequested(
                        turn_id=session.turn_id,
                        timer_id=timer_id,
                        fire_at_unix_ms=schedule_request.fire_at_unix_ms,
                        label=schedule_request.label,
                        duration_ms=schedule_request.duration_ms,
                    )
                )
            return IntentResolved(
                turn_id=session.turn_id,
                intent_name=schedule_request.intent_name,
                reply_text=schedule_request.reply_text,
                resolution_reason="offline_first",
            )

        quiet_request = _parse_quiet_mode_request(normalized)
        if quiet_request is not None:
            if self.event_publisher is not None:
                self.event_publisher(
                    QuietModeSetRequested(
                        turn_id=session.turn_id,
                        enabled=quiet_request.enabled,
                    )
                )
            return IntentResolved(
                turn_id=session.turn_id,
                intent_name=quiet_request.intent_name,
                reply_text=quiet_request.reply_text,
                resolution_reason="offline_first",
            )

        volume_request = _parse_volume_request(normalized)
        if volume_request is not None:
            if self.event_publisher is not None:
                self.event_publisher(
                    VolumeSetRequested(
                        turn_id=session.turn_id,
                        volume_percent=volume_request.volume_percent,
                    )
                )
            return IntentResolved(
                turn_id=session.turn_id,
                intent_name=volume_request.intent_name,
                reply_text=volume_request.reply_text,
                resolution_reason="offline_first",
            )

        if _matches_volume_query(normalized):
            return IntentResolved(
                turn_id=session.turn_id,
                intent_name="local_volume_guidance",
                reply_text="Consigo ajustar volume com valor explicito, por exemplo volume 40, e ligar ou desligar o modo silencioso.",
                resolution_reason="offline_first_guidance",
            )

        return None


def _normalize(text: str) -> str:
    normalized = text.casefold()
    replacements = str.maketrans(
        {
            "á": "a",
            "à": "a",
            "â": "a",
            "ã": "a",
            "é": "e",
            "ê": "e",
            "í": "i",
            "ó": "o",
            "ô": "o",
            "õ": "o",
            "ú": "u",
            "ç": "c",
        }
    )
    normalized = normalized.translate(replacements)
    normalized = re.sub(r"[^a-z0-9\s]", " ", normalized)
    return re.sub(r"\s+", " ", normalized).strip()


def _matches_time_query(text: str) -> bool:
    patterns = (
        r"\bque horas\b",
        r"\bhoras sao\b",
        r"\bme diga a hora\b",
        r"\bqual a hora\b",
        r"\bqual e a hora\b",
    )
    return any(re.search(pattern, text) for pattern in patterns)


def _matches_date_query(text: str) -> bool:
    patterns = (
        r"\bque dia\b",
        r"\bdata de hoje\b",
        r"\bqual e a data\b",
        r"\bqual a data\b",
        r"\bque dia e hoje\b",
        r"\bque data e hoje\b",
    )
    return any(re.search(pattern, text) for pattern in patterns)


def _matches_status_query(text: str) -> bool:
    patterns = (
        r"\bstatus\b",
        r"\bcomo voce esta\b",
        r"\besta online\b",
        r"\besta operacional\b",
        r"\bstatus do sistema\b",
    )
    return any(re.search(pattern, text) for pattern in patterns)


def _matches_timer_query(text: str) -> bool:
    return any(keyword in text for keyword in ("timer", "alarme", "lembrete"))


def _matches_volume_query(text: str) -> bool:
    return any(keyword in text for keyword in ("volume", "silencioso", "mute", "mudo"))


@dataclass(frozen=True)
class _ScheduleRequest:
    intent_name: str
    fire_at_unix_ms: int
    duration_ms: int
    label: str
    reply_text: str


@dataclass(frozen=True)
class _QuietModeRequest:
    intent_name: str
    enabled: bool
    reply_text: str


@dataclass(frozen=True)
class _VolumeRequest:
    intent_name: str
    volume_percent: int
    reply_text: str


def _parse_schedule_request(text: str, now: datetime) -> _ScheduleRequest | None:
    alarm_request = _parse_alarm_request(text, now)
    if alarm_request is not None:
        return alarm_request

    return _parse_timer_request(text, now)


def _parse_timer_request(text: str, now: datetime) -> _ScheduleRequest | None:
    if not _matches_timer_query(text):
        return None

    match = re.search(
        r"\b(?P<value>\d+)\s*(?P<unit>segundo|segundos|minuto|minutos|hora|horas)\b",
        text,
    )
    if match is None:
        return None

    value = int(match.group("value"))
    unit = match.group("unit")
    factors = {
        "segundo": 1000,
        "segundos": 1000,
        "minuto": 60_000,
        "minutos": 60_000,
        "hora": 3_600_000,
        "horas": 3_600_000,
    }
    duration_ms = value * factors[unit]
    fire_at_unix_ms = int(now.timestamp() * 1000.0) + duration_ms
    return _ScheduleRequest(
        intent_name="local_timer_create",
        fire_at_unix_ms=fire_at_unix_ms,
        duration_ms=duration_ms,
        label=f"timer {value} {unit}",
        reply_text=_build_timer_reply(now, duration_ms),
    )


def _parse_alarm_request(text: str, now: datetime) -> _ScheduleRequest | None:
    if "alarme" not in text:
        return None

    patterns = (
        r"\b(?:(?P<prefix>para as|as|para)\s+)(?P<hour>\d{1,2})\s+(?P<minute>\d{2})\b",
        r"\b(?:(?P<prefix>para as|as|para)\s+)?(?P<hour>\d{1,2})\s*(?P<suffix>h|hora|horas)\b",
    )
    match = None
    for pattern in patterns:
        match = re.search(pattern, text)
        if match is not None:
            break
    if match is None:
        return None

    groups = match.groupdict()
    prefix = groups.get("prefix")
    minute_text = groups.get("minute")
    suffix = groups.get("suffix")
    if prefix is None and minute_text is None and suffix is None:
        return None

    hour = int(match.group("hour"))
    minute = int(minute_text or 0)
    if hour > 23 or minute > 59:
        return None

    fire_at = now.replace(hour=hour, minute=minute, second=0, microsecond=0)
    if fire_at <= now:
        fire_at = fire_at + timedelta(days=1)

    duration_ms = int((fire_at.timestamp() - now.timestamp()) * 1000.0)
    label = f"alarme {hour:02d}:{minute:02d}"
    return _ScheduleRequest(
        intent_name="local_alarm_create",
        fire_at_unix_ms=int(fire_at.timestamp() * 1000.0),
        duration_ms=duration_ms,
        label=label,
        reply_text=f"Alarme criado para {fire_at.strftime('%H:%M')}.",
    )


def _parse_quiet_mode_request(text: str) -> _QuietModeRequest | None:
    quiet_keywords = ("modo silencioso", "silencioso", "quiet")
    if not any(keyword in text for keyword in quiet_keywords):
        return None

    disable_patterns = (
        r"\b(desative|desligue|saia do|tire do|remova o)\s+(modo\s+)?silencioso\b",
        r"\bquiet\b.*\b(off|desligado)\b",
    )
    if any(re.search(pattern, text) for pattern in disable_patterns):
        return _QuietModeRequest(
            intent_name="local_quiet_mode_disable",
            enabled=False,
            reply_text="Modo silencioso desativado.",
        )

    enable_patterns = (
        r"\b(ative|ligue|entre em|coloque em)\s+(modo\s+)?silencioso\b",
        r"\bficar?\s+silencioso\b",
        r"\bmodo silencioso\b",
        r"\bquiet\b",
    )
    if any(re.search(pattern, text) for pattern in enable_patterns):
        return _QuietModeRequest(
            intent_name="local_quiet_mode_enable",
            enabled=True,
            reply_text="Modo silencioso ativado.",
        )
    return None


def _parse_volume_request(text: str) -> _VolumeRequest | None:
    if any(keyword in text for keyword in ("mute", "mudo", "sem som")):
        return _VolumeRequest(
            intent_name="local_volume_set",
            volume_percent=0,
            reply_text="Volume ajustado para 0%.",
        )

    if "volume" not in text:
        return None

    match = re.search(
        r"\bvolume(?:\s+(?:para|em))?\s+(?P<value>\d{1,3})\b",
        text,
    )
    if match is None:
        return None

    value = int(match.group("value"))
    if value < 0 or value > 100:
        return None

    return _VolumeRequest(
        intent_name="local_volume_set",
        volume_percent=value,
        reply_text=f"Volume ajustado para {value}%.",
    )


def _build_timer_reply(now: datetime, duration_ms: int) -> str:
    fire_at = datetime.fromtimestamp(
        now.timestamp() + (duration_ms / 1000.0),
        tz=now.tzinfo,
    )
    duration_s = duration_ms // 1000
    if duration_s % 3600 == 0:
        value = duration_s // 3600
        unit = "hora" if value == 1 else "horas"
    elif duration_s % 60 == 0:
        value = duration_s // 60
        unit = "minuto" if value == 1 else "minutos"
    else:
        value = duration_s
        unit = "segundo" if value == 1 else "segundos"
    return f"Timer de {value} {unit} criado para {fire_at.strftime('%H:%M:%S')}."
