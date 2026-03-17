# ESP32 Vehicle Safety Dashboard

FastAPI dashboard for an ESP32-based vehicle safety mini-project. The app reads live sensor lines from `/dev/tty.usbserial-55980077511` and visualizes front and rear distance sensing, buzzer/RGB states, acceleration, delta acceleration, and accident conditions.

## Features

- FastAPI backend with serial-port ingestion and API ingestion endpoint
- Parser that converts serial log lines into structured JSON
- API endpoints:
  - `POST /sensor-data`
  - `GET /latest`
  - `GET /history`
  - `GET /config`
  - `GET /ws` websocket feed
- Browser dashboard with:
  - top-down car UI
  - front and rear radar-wave warnings
  - telemetry cards
  - recent log list
  - trend chart

## Run

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
python -m uvicorn app.main:app --reload
```

Open `http://127.0.0.1:8000`.

## GitHub Pages Demo

This repository also includes a static demo entry page at `/index.html` for GitHub Pages.
It reuses the same dashboard UI but plays bundled mock readings from `app/static/data/demo-data.json`.

The GitHub Pages site is deployed by [`.github/workflows/deploy-pages.yml`](/Users/tree/embedded2/.github/workflows/deploy-pages.yml).
Use GitHub Pages for the demo UI only; the FastAPI backend, websocket feed, and serial port ingestion still need to run locally or on a server that supports Python processes.

The dashboard reads from serial port `/dev/tty.usbserial-55980077511` at `115200` baud by default.
Close any other serial monitor that is already using that port before starting the app.

If `uvicorn` resolves to a global Python instead of `.venv`, prefer:

```bash
.venv/bin/python -m uvicorn app.main:app --reload
```

## Send Sensor Data Manually

You can post raw serial lines directly:

```bash
curl -X POST http://127.0.0.1:8000/sensor-data \
  -H "Content-Type: text/plain" \
  --data 'S1=15.2 S2=N/A Near=15.2 Buzz=FAST RGB=RED_BLINK AX=-0.137 dA=0.000 Alert=OFF'
```

Or send JSON:

```bash
curl -X POST http://127.0.0.1:8000/sensor-data \
  -H "Content-Type: application/json" \
  -d '{"s1": 12.4, "s2": null, "buzz": "FAST", "rgb": "RED_BLINK", "ax": 0.132, "da": 0.441, "alert": "OFF"}'
```

## Thresholds

Update `app/config.py` to tune:

- serial port settings
- distance zones
- accident `dA` threshold
- whether `dA` needs distance confirmation
- max distance allowed for `dA`-based accident detection
- accident keywords
- polling and mock intervals
