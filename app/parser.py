from datetime import datetime, timezone

from .config import DashboardConfig, settings
from .models import SensorDataRequest, SensorReading, ZoneLevel


NO_DATA_MARKERS = {"", "N/A", "NR", "NONE", "NULL", "NO_DATA"}


def parse_numeric(value: str | float | int | None) -> float | None:
    if value is None:
        return None
    if isinstance(value, (float, int)):
        return float(value)

    cleaned = value.strip()
    if cleaned.upper() in NO_DATA_MARKERS:
        return None

    return float(cleaned)


def normalize_text(value: str | None, fallback: str = "OFF") -> str:
    cleaned = (value or fallback).strip()
    if not cleaned:
        return fallback
    return cleaned.upper()


def resolve_nearest_distance(near: float | None, s1: float | None, s2: float | None) -> float | None:
    if near is not None:
        return near

    valid_values = [value for value in (s1, s2) if value is not None]
    if not valid_values:
        return None

    return min(valid_values)


def classify_distance(distance_cm: float | None, config: DashboardConfig = settings) -> ZoneLevel:
    if distance_cm is None:
        return "no_data"

    thresholds = config.distance_thresholds
    if distance_cm <= thresholds.accident_cm:
        return "accident"
    if distance_cm <= thresholds.danger_cm:
        return "danger"
    if distance_cm <= thresholds.warning_cm:
        return "warning"
    if distance_cm <= thresholds.caution_cm:
        return "caution"
    return "safe"


def detect_accident(
    alert: str,
    da: float | None,
    near: float | None,
    config: DashboardConfig = settings,
) -> tuple[bool, list[str]]:
    reasons: list[str] = []

    if any(keyword in alert for keyword in config.accident_keywords):
        reasons.append(f"Alert state reported {alert}")

    if da is not None and abs(da) >= config.accident_delta_threshold:
        if (
            not config.require_distance_for_delta_accident
            or (near is not None and near <= config.delta_accident_distance_cm)
        ):
            reasons.append(
                "dA threshold exceeded "
                f"({abs(da):.3f} >= {config.accident_delta_threshold:.3f}) "
                f"with near confirmation at {near:.1f} cm"
            )

    return bool(reasons), reasons


def parse_sensor_line(
    line: str,
    config: DashboardConfig = settings,
    *,
    source: str = "mock",
    timestamp: datetime | None = None,
) -> SensorReading:
    raw_line = line.strip()
    if not raw_line:
        raise ValueError("Sensor log line is empty.")

    tokens: dict[str, str] = {}
    for segment in raw_line.split():
        if "=" not in segment:
            continue
        key, value = segment.split("=", 1)
        tokens[key.strip().upper()] = value.strip()

    if not tokens:
        raise ValueError("No key=value pairs found in sensor log line.")

    s1 = parse_numeric(tokens.get("S1"))
    s2 = parse_numeric(tokens.get("S2"))
    near = resolve_nearest_distance(parse_numeric(tokens.get("NEAR")), s1, s2)
    buzz = normalize_text(tokens.get("BUZZ"))
    rgb = normalize_text(tokens.get("RGB"))
    ax = parse_numeric(tokens.get("AX"))
    da = parse_numeric(tokens.get("DA"))
    alert = normalize_text(tokens.get("ALERT"))
    accident, reasons = detect_accident(alert, da, near, config)

    return SensorReading(
        timestamp=timestamp or datetime.now(timezone.utc),
        raw_line=raw_line,
        source=source,
        s1=s1,
        s2=s2,
        near=near,
        buzz=buzz,
        rgb=rgb,
        ax=ax,
        da=da,
        alert=alert,
        front_zone=classify_distance(s1, config),
        rear_zone=classify_distance(s2, config),
        near_zone=classify_distance(near, config),
        accident=accident,
        accident_reason=reasons,
    )


def build_raw_line_from_payload(payload: SensorDataRequest) -> str:
    def value_or_na(value: float | None, digits: int = 1) -> str:
        if value is None:
            return "N/A"
        return f"{value:.{digits}f}"

    ax_text = value_or_na(payload.ax, 3)
    da_text = value_or_na(payload.da, 3)
    near = resolve_nearest_distance(payload.near, payload.s1, payload.s2)

    return (
        f"S1={value_or_na(payload.s1)} "
        f"S2={value_or_na(payload.s2)} "
        f"Near={value_or_na(near)} "
        f"Buzz={normalize_text(payload.buzz)} "
        f"RGB={normalize_text(payload.rgb)} "
        f"AX={ax_text} "
        f"dA={da_text} "
        f"Alert={normalize_text(payload.alert)}"
    )


def build_reading_from_payload(
    payload: SensorDataRequest,
    config: DashboardConfig = settings,
) -> SensorReading:
    near = resolve_nearest_distance(payload.near, payload.s1, payload.s2)
    alert = normalize_text(payload.alert)
    accident, reasons = detect_accident(alert, payload.da, near, config)

    return SensorReading(
        timestamp=payload.timestamp or datetime.now(timezone.utc),
        raw_line=payload.line or build_raw_line_from_payload(payload),
        source=payload.source,
        s1=payload.s1,
        s2=payload.s2,
        near=near,
        buzz=normalize_text(payload.buzz),
        rgb=normalize_text(payload.rgb),
        ax=payload.ax,
        da=payload.da,
        alert=alert,
        front_zone=classify_distance(payload.s1, config),
        rear_zone=classify_distance(payload.s2, config),
        near_zone=classify_distance(near, config),
        accident=accident,
        accident_reason=reasons,
    )
