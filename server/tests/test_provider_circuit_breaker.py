from __future__ import annotations

import pytest

from noisebot2.providers import CircuitBreaker, CircuitOpenError, CircuitState


def test_circuit_breaker_opens_after_threshold() -> None:
    now = 100.0

    def clock() -> float:
        return now

    breaker = CircuitBreaker(provider="llm/test", failure_threshold=2, reset_timeout=5.0, clock=clock)

    assert breaker.allow_request() is True
    breaker.record_failure()
    assert breaker.state == CircuitState.CLOSED

    assert breaker.allow_request() is True
    breaker.record_failure()
    assert breaker.state == CircuitState.OPEN

    with pytest.raises(CircuitOpenError):
        breaker.allow_request()


def test_circuit_breaker_half_open_then_closes_on_success() -> None:
    now = 100.0

    def clock() -> float:
        return now

    breaker = CircuitBreaker(provider="llm/test", failure_threshold=1, reset_timeout=5.0, clock=clock)
    breaker.record_failure()
    assert breaker.state == CircuitState.OPEN

    now = 106.0
    assert breaker.allow_request() is True
    assert breaker.state == CircuitState.HALF_OPEN
    breaker.record_success()
    assert breaker.state == CircuitState.CLOSED
