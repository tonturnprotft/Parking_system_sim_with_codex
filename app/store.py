import asyncio
from collections import deque

from .models import SensorReading


class SensorStore:
    def __init__(self, history_limit: int) -> None:
        self._history: deque[SensorReading] = deque(maxlen=history_limit)
        self._latest: SensorReading | None = None
        self._lock = asyncio.Lock()
        self._listeners: set[asyncio.Queue[SensorReading]] = set()

    async def add_reading(self, reading: SensorReading) -> None:
        async with self._lock:
            self._latest = reading
            self._history.append(reading)
            listeners = list(self._listeners)

        for queue in listeners:
            if queue.full():
                try:
                    queue.get_nowait()
                except asyncio.QueueEmpty:
                    pass
            queue.put_nowait(reading)

    async def get_latest(self) -> SensorReading | None:
        async with self._lock:
            return self._latest

    async def get_history(self, limit: int | None = None) -> list[SensorReading]:
        async with self._lock:
            items = list(self._history)

        if limit is None or limit >= len(items):
            return items
        return items[-limit:]

    async def subscribe(self) -> asyncio.Queue[SensorReading]:
        queue: asyncio.Queue[SensorReading] = asyncio.Queue(maxsize=12)
        async with self._lock:
            self._listeners.add(queue)
            latest = self._latest

        if latest is not None:
            queue.put_nowait(latest)
        return queue

    async def unsubscribe(self, queue: asyncio.Queue[SensorReading]) -> None:
        async with self._lock:
            self._listeners.discard(queue)

