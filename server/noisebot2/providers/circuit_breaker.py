"""Circuit breaker simples para providers do server."""

from __future__ import annotations

import time
from enum import Enum


class CircuitState(Enum):
    CLOSED = "closed"
    OPEN = "open"
    HALF_OPEN = "half_open"


class CircuitOpenError(RuntimeError):
    def __init__(self, provider: str, reset_in: float = 0.0) -> None:
        self.provider = provider
        self.reset_in = reset_in
        super().__init__(f"Circuit OPEN para '{provider}'. Proxima sonda em {reset_in:.1f}s")


class CircuitBreaker:
    def __init__(
        self,
        provider: str = "unknown",
        failure_threshold: int = 3,
        reset_timeout: float = 30.0,
        *,
        clock: callable | None = None,
    ) -> None:
        self._provider = provider
        self._failure_threshold = max(1, failure_threshold)
        self._reset_timeout = max(0.0, reset_timeout)
        self._clock = clock or time.monotonic
        self._state = CircuitState.CLOSED
        self._failure_count = 0
        self._opened_at = 0.0
        self._probe_active = False

    @property
    def state(self) -> CircuitState:
        self._maybe_half_open()
        return self._state

    @property
    def failure_count(self) -> int:
        return self._failure_count

    def allow_request(self) -> bool:
        self._maybe_half_open()
        if self._state == CircuitState.CLOSED:
            return True
        if self._state == CircuitState.HALF_OPEN and not self._probe_active:
            self._probe_active = True
            return True
        raise CircuitOpenError(self._provider, self._time_until_probe())

    def record_success(self) -> None:
        self._failure_count = 0
        self._probe_active = False
        self._state = CircuitState.CLOSED

    def record_failure(self) -> None:
        self._failure_count += 1
        self._probe_active = False
        self._opened_at = self._clock()
        if self._failure_count >= self._failure_threshold:
            self._state = CircuitState.OPEN

    def reset(self) -> None:
        self._state = CircuitState.CLOSED
        self._failure_count = 0
        self._opened_at = 0.0
        self._probe_active = False

    def _maybe_half_open(self) -> None:
        if (
            self._state == CircuitState.OPEN
            and self._opened_at > 0.0
            and self._clock() - self._opened_at >= self._reset_timeout
        ):
            self._state = CircuitState.HALF_OPEN
            self._probe_active = False

    def _time_until_probe(self) -> float:
        if self._opened_at <= 0.0:
            return self._reset_timeout
        return max(0.0, self._reset_timeout - (self._clock() - self._opened_at))
