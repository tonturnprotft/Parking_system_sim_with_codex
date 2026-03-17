from dataclasses import dataclass, field


@dataclass(slots=True)
class DistanceThresholds:
    caution_cm: float = 40.0
    warning_cm: float = 20.0
    danger_cm: float = 10.0
    accident_cm: float = 5.0


@dataclass(slots=True)
class DashboardConfig:
    project_title: str = "ESP32 Vehicle Safety Dashboard"
    mock_mode: bool = False
    mock_interval_seconds: float = 0.5
    serial_port: str = "/dev/tty.usbserial-55980077511"
    serial_baud_rate: int = 115200
    serial_timeout_seconds: float = 0.1
    serial_reconnect_seconds: float = 0.5
    history_limit: int = 120
    default_history_points: int = 32
    poll_interval_ms: int = 250
    accident_delta_threshold: float = 0.7
    require_distance_for_delta_accident: bool = True
    delta_accident_distance_cm: float = 5.0
    accident_keywords: tuple[str, ...] = ("IMPACT", "ACCIDENT", "COLLISION")
    distance_thresholds: DistanceThresholds = field(default_factory=DistanceThresholds)

    def public_dict(self) -> dict[str, object]:
        return {
            "project_title": self.project_title,
            "mock_mode": self.mock_mode,
            "mock_interval_seconds": self.mock_interval_seconds,
            "serial_port": self.serial_port,
            "serial_baud_rate": self.serial_baud_rate,
            "serial_timeout_seconds": self.serial_timeout_seconds,
            "serial_reconnect_seconds": self.serial_reconnect_seconds,
            "history_limit": self.history_limit,
            "default_history_points": self.default_history_points,
            "poll_interval_ms": self.poll_interval_ms,
            "accident_delta_threshold": self.accident_delta_threshold,
            "require_distance_for_delta_accident": self.require_distance_for_delta_accident,
            "delta_accident_distance_cm": self.delta_accident_distance_cm,
            "accident_keywords": list(self.accident_keywords),
            "distance_thresholds": {
                "caution_cm": self.distance_thresholds.caution_cm,
                "warning_cm": self.distance_thresholds.warning_cm,
                "danger_cm": self.distance_thresholds.danger_cm,
                "accident_cm": self.distance_thresholds.accident_cm,
            },
        }


settings = DashboardConfig()
