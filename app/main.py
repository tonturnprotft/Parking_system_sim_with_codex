import asyncio
import json
import logging
from contextlib import asynccontextmanager, suppress
from pathlib import Path

from fastapi import FastAPI, HTTPException, Request, WebSocket, WebSocketDisconnect
from fastapi.responses import HTMLResponse
from fastapi.staticfiles import StaticFiles
from fastapi.templating import Jinja2Templates

from .config import settings
from .mock_data import MockSensorStream
from .models import ConfigResponse, HistoryResponse, IngestResponse, LatestResponse, SensorDataRequest
from .parser import build_reading_from_payload, parse_sensor_line
from .serial_stream import SerialSensorStream
from .store import SensorStore


BASE_DIR = Path(__file__).resolve().parent
templates = Jinja2Templates(directory=str(BASE_DIR / "templates"))
logger = logging.getLogger(__name__)


@asynccontextmanager
async def lifespan(app: FastAPI):
    store = SensorStore(settings.history_limit)
    app.state.store = store

    stream_task: asyncio.Task[None] | None = None
    if settings.mock_mode:
        mock_stream = MockSensorStream(store, settings)
        stream_task = asyncio.create_task(mock_stream.run())
        logger.info("Dashboard source: mock stream")
    else:
        serial_stream = SerialSensorStream(store, settings)
        stream_task = asyncio.create_task(serial_stream.run())
        logger.info("Dashboard source: serial port %s", settings.serial_port)

    try:
        yield
    finally:
        if stream_task is not None:
            stream_task.cancel()
            with suppress(asyncio.CancelledError):
                await stream_task


app = FastAPI(title=settings.project_title, lifespan=lifespan)
app.mount("/static", StaticFiles(directory=str(BASE_DIR / "static")), name="static")


@app.get("/", response_class=HTMLResponse)
async def dashboard(request: Request) -> HTMLResponse:
    return templates.TemplateResponse(
        "index.html",
        {
            "request": request,
            "page_title": settings.project_title,
            "config_json": json.dumps(settings.public_dict()),
        },
    )


@app.post("/sensor-data", response_model=IngestResponse)
async def post_sensor_data(request: Request) -> IngestResponse:
    body = await request.body()
    if not body:
        raise HTTPException(status_code=400, detail="Request body is empty.")

    content_type = request.headers.get("content-type", "")
    if "application/json" in content_type:
        try:
            payload = SensorDataRequest.model_validate_json(body)
        except Exception as exc:  # pragma: no cover - FastAPI error shaping
            raise HTTPException(status_code=400, detail=f"Invalid JSON payload: {exc}") from exc
    else:
        payload = SensorDataRequest(line=body.decode("utf-8").strip(), source="serial")

    try:
        reading = (
            parse_sensor_line(payload.line, settings, source=payload.source, timestamp=payload.timestamp)
            if payload.line
            else build_reading_from_payload(payload, settings)
        )
    except ValueError as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc

    await request.app.state.store.add_reading(reading)
    return IngestResponse(status="accepted", data=reading)


@app.get("/latest", response_model=LatestResponse)
async def get_latest(request: Request) -> LatestResponse:
    latest = await request.app.state.store.get_latest()
    mode = "mock" if settings.mock_mode else "serial"
    return LatestResponse(mode=mode, data=latest)


@app.get("/history", response_model=HistoryResponse)
async def get_history(request: Request, limit: int | None = None) -> HistoryResponse:
    if limit is not None and limit <= 0:
        raise HTTPException(status_code=400, detail="limit must be greater than zero.")

    resolved_limit = limit or settings.default_history_points
    items = await request.app.state.store.get_history(resolved_limit)
    return HistoryResponse(count=len(items), items=items)


@app.get("/config", response_model=ConfigResponse)
async def get_config() -> ConfigResponse:
    return ConfigResponse(**settings.public_dict())


@app.websocket("/ws")
async def sensor_feed(websocket: WebSocket) -> None:
    await websocket.accept()
    queue = await websocket.app.state.store.subscribe()

    try:
        while True:
            reading = await queue.get()
            await websocket.send_json(reading.model_dump(mode="json"))
    except WebSocketDisconnect:
        pass
    finally:
        await websocket.app.state.store.unsubscribe(queue)
