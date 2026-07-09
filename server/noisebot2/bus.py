"""Barramento interno tipado da mente."""

from __future__ import annotations

import asyncio
import logging
from typing import Any

log = logging.getLogger(__name__)

_SENTINEL = object()


class EventBus:
    """Barramento assíncrono com fila por assinante."""

    def __init__(self, default_maxsize: int = 128) -> None:
        self._default_maxsize = default_maxsize
        self._subscribers: list[tuple[tuple[type[Any], ...], asyncio.Queue[Any]]] = []
        self._closed = False

    def subscribe(self, *event_types: type[Any], maxsize: int = 0) -> asyncio.Queue[Any]:
        size = self._default_maxsize if maxsize == 0 else maxsize
        if size < 0:
            size = 0
        queue: asyncio.Queue[Any] = asyncio.Queue(maxsize=size)
        self._subscribers.append((event_types, queue))
        return queue

    def unsubscribe(self, queue: asyncio.Queue[Any]) -> None:
        self._subscribers = [
            (types, subscriber)
            for types, subscriber in self._subscribers
            if subscriber is not queue
        ]

    async def publish(self, event: Any) -> None:
        self.publish_nowait(event)

    def publish_nowait(self, event: Any) -> None:
        if self._closed:
            return

        for event_types, queue in self._subscribers:
            if event_types and not isinstance(event, event_types):
                continue
            try:
                queue.put_nowait(event)
            except asyncio.QueueFull:
                log.warning("bus: fila cheia, descartando %s", type(event).__name__)

    async def close(self) -> None:
        self._closed = True
        for _, queue in self._subscribers:
            try:
                queue.put_nowait(_SENTINEL)
            except asyncio.QueueFull:
                pass
        self._subscribers.clear()

    @staticmethod
    async def iter_queue(queue: asyncio.Queue[Any]):
        while True:
            event = await queue.get()
            try:
                if event is _SENTINEL:
                    break
                yield event
            finally:
                queue.task_done()
