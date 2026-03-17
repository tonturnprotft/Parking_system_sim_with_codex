import asyncio
import logging
from typing import Any

from .config import DashboardConfig
from .parser import parse_sensor_line
from .store import SensorStore


logger = logging.getLogger(__name__)


class SerialSensorStream:
    def __init__(self, store: SensorStore, config: DashboardConfig) -> None:
        self._store = store
        self._config = config

    async def run(self) -> None:
        while True:
            try:
                await self._read_loop()
            except asyncio.CancelledError:
                raise
            except Exception as exc:  # pragma: no cover - runtime hardware behavior
                logger.warning("Serial stream disconnected: %s", exc)
                await asyncio.sleep(self._config.serial_reconnect_seconds)

    async def _read_loop(self) -> None:
        serial_module = self._import_serial_module()
        connection = await asyncio.to_thread(
            serial_module.Serial,
            self._config.serial_port,
            self._config.serial_baud_rate,
            timeout=self._config.serial_timeout_seconds,
        )

        logger.info(
            "Reading sensor data from %s at %s baud",
            self._config.serial_port,
            self._config.serial_baud_rate,
        )

        try:
            while True:
                raw_bytes = await asyncio.to_thread(connection.readline)
                if not raw_bytes:
                    continue

                raw_line = raw_bytes.decode("utf-8", errors="ignore").strip()
                if not raw_line:
                    continue

                try:
                    reading = parse_sensor_line(raw_line, self._config, source="serial")
                except ValueError:
                    logger.debug("Skipping unparsable serial line: %s", raw_line)
                    continue

                await self._store.add_reading(reading)
        finally:
            await asyncio.to_thread(connection.close)

    @staticmethod
    def _import_serial_module() -> Any:
        try:
            import serial  # type: ignore
        except ModuleNotFoundError as exc:  # pragma: no cover - dependency issue
            raise RuntimeError(
                "pyserial is not installed. Run `.venv/bin/python -m pip install -r requirements.txt`."
            ) from exc

        return serial
