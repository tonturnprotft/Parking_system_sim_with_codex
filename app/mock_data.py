import asyncio

from .config import DashboardConfig
from .parser import parse_sensor_line
from .store import SensorStore


SAMPLE_LOG_LINES = [
    "S1=15.2 S2=N/A Near=15.2 Buzz=FAST RGB=RED_BLINK AX=-0.137 dA=0.000 Alert=OFF",
    "S1=17.6 S2=N/A Near=17.6 Buzz=FAST RGB=RED_BLINK AX=0.004 dA=0.000 Alert=OFF",
    "S1=17.2 S2=N/A Near=17.2 Buzz=FAST RGB=RED_BLINK AX=0.398 dA=0.395 Alert=OFF",
    "S1=N/A S2=N/A Near=N/A Buzz=OFF RGB=OFF AX=-1.133 dA=-0.770 Alert=OFF",
    "S1=N/A S2=N/A Near=N/A Buzz=OFF RGB=OFF AX=0.004 dA=0.031 Alert=OFF",
    "S1=N/A S2=N/A Near=N/A Buzz=OFF RGB=OFF AX=-0.004 dA=-0.004 Alert=OFF",
    "S1=8.7 S2=N/A Near=8.7 Buzz=CONTINUOUS RGB=RED AX=0.148 dA=0.145 Alert=OFF",
    "S1=4.2 S2=12.8 Near=4.2 Buzz=CONTINUOUS RGB=RED AX=1.240 dA=0.955 Alert=IMPACT",
    "S1=N/A S2=36.4 Near=36.4 Buzz=SLOW RGB=YELLOW_BLINK AX=-0.054 dA=0.012 Alert=OFF",
    "S1=N/A S2=9.6 Near=9.6 Buzz=FAST RGB=RED_BLINK AX=-0.120 dA=-0.210 Alert=OFF",
]


class MockSensorStream:
    def __init__(self, store: SensorStore, config: DashboardConfig) -> None:
        self._store = store
        self._config = config
        self._cursor = 0

    async def run(self) -> None:
        while True:
            line = SAMPLE_LOG_LINES[self._cursor % len(SAMPLE_LOG_LINES)]
            reading = parse_sensor_line(line, self._config, source="mock")
            await self._store.add_reading(reading)
            self._cursor += 1
            await asyncio.sleep(self._config.mock_interval_seconds)

