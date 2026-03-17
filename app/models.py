from datetime import datetime, timezone
from typing import Literal

from pydantic import BaseModel, ConfigDict, Field


ZoneLevel = Literal["no_data", "safe", "caution", "warning", "danger", "accident"]


class SensorReading(BaseModel):
    model_config = ConfigDict(str_strip_whitespace=True)

    timestamp: datetime = Field(default_factory=lambda: datetime.now(timezone.utc))
    raw_line: str
    source: str = "mock"
    s1: float | None = None
    s2: float | None = None
    near: float | None = None
    buzz: str = "OFF"
    rgb: str = "OFF"
    ax: float | None = None
    da: float | None = None
    alert: str = "OFF"
    front_zone: ZoneLevel = "no_data"
    rear_zone: ZoneLevel = "no_data"
    near_zone: ZoneLevel = "no_data"
    accident: bool = False
    accident_reason: list[str] = Field(default_factory=list)


class SensorDataRequest(BaseModel):
    model_config = ConfigDict(extra="ignore")

    line: str | None = None
    source: str = "api"
    timestamp: datetime | None = None
    s1: float | None = None
    s2: float | None = None
    near: float | None = None
    buzz: str | None = None
    rgb: str | None = None
    ax: float | None = None
    da: float | None = None
    alert: str | None = None


class IngestResponse(BaseModel):
    status: str
    data: SensorReading


class LatestResponse(BaseModel):
    mode: str
    data: SensorReading | None


class HistoryResponse(BaseModel):
    count: int
    items: list[SensorReading]


class ConfigResponse(BaseModel):
    project_title: str
    mock_mode: bool
    mock_interval_seconds: float
    serial_port: str
    serial_baud_rate: int
    serial_timeout_seconds: float
    serial_reconnect_seconds: float
    history_limit: int
    default_history_points: int
    poll_interval_ms: int
    accident_delta_threshold: float
    require_distance_for_delta_accident: bool
    delta_accident_distance_cm: float
    accident_keywords: list[str]
    distance_thresholds: dict[str, float]
