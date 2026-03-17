const config = JSON.parse(document.getElementById("dashboard-config").textContent);

const zoneLabels = {
  no_data: "No Data",
  safe: "Safe",
  caution: "Caution",
  warning: "Warning",
  danger: "Danger",
  accident: "Accident",
};

const toneByZone = {
  no_data: "neutral",
  safe: "safe",
  caution: "caution",
  warning: "warning",
  danger: "danger",
  accident: "danger",
};

const state = {
  latestTimestamp: null,
  pollTimer: null,
  charts: {
    vl53: null,
    adxl: null,
  },
  connectionMode: "Connecting",
};

const ui = {
  modeChip: document.getElementById("modeChip"),
  feedModeChip: document.getElementById("feedModeChip"),
  lastUpdateText: document.getElementById("lastUpdateText"),
  nearPill: document.getElementById("nearPill"),
  frontSensorText: document.getElementById("frontSensorText"),
  rearSensorText: document.getElementById("rearSensorText"),
  frontRadar: document.getElementById("frontRadar"),
  rearRadar: document.getElementById("rearRadar"),
  alertBanner: document.getElementById("alertBanner"),
  vehicleAlertWindow: document.getElementById("vehicleAlertWindow"),
  vehicleAlertTitle: document.getElementById("vehicleAlertTitle"),
  vehicleAlertReason: document.getElementById("vehicleAlertReason"),
  logList: document.getElementById("logList"),
  cards: {
    front: document.getElementById("cardFront"),
    rear: document.getElementById("cardRear"),
    ax: document.getElementById("cardAx"),
    da: document.getElementById("cardDa"),
    buzz: document.getElementById("cardBuzz"),
    rgb: document.getElementById("cardRgb"),
    alert: document.getElementById("cardAlert"),
  },
  values: {
    front: document.getElementById("cardFrontValue"),
    rear: document.getElementById("cardRearValue"),
    ax: document.getElementById("cardAxValue"),
    da: document.getElementById("cardDaValue"),
    buzz: document.getElementById("cardBuzzValue"),
    rgb: document.getElementById("cardRgbValue"),
    alert: document.getElementById("cardAlertValue"),
  },
  meta: {
    front: document.getElementById("cardFrontMeta"),
    rear: document.getElementById("cardRearMeta"),
    da: document.getElementById("cardDaMeta"),
    alert: document.getElementById("cardAlertMeta"),
  },
};

ui.modeChip.textContent = config.mock_mode ? "Mock Mode" : "Live Mode";

function formatDistance(value) {
  if (value === null || value === undefined) {
    return "No Data";
  }
  return `${Number(value).toFixed(1)} cm`;
}

function formatNumber(value) {
  if (value === null || value === undefined) {
    return "No Data";
  }
  return Number(value).toFixed(3);
}

function formatTimestamp(value) {
  if (!value) {
    return "Waiting for data...";
  }

  const date = new Date(value);
  return date.toLocaleTimeString([], {
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit",
  });
}

function setConnectionMode(label) {
  state.connectionMode = label;
  ui.feedModeChip.textContent = label;
}

function toneFromReadingZone(zone) {
  return toneByZone[zone] || "neutral";
}

function setCardTone(card, tone) {
  card.dataset.tone = tone;
}

function renderRadar(radarElement, zone, value) {
  radarElement.dataset.zone = zone || "no_data";
  radarElement.setAttribute("aria-label", `${zoneLabels[zone] || "No Data"} radar`);
  radarElement.title = value === null || value === undefined
    ? "No Data"
    : `${zoneLabels[zone] || "No Data"} - ${Number(value).toFixed(1)} cm`;
}

function updateVehicleAlert(reading) {
  const normalizedAlert = String(reading.alert || "OFF").toUpperCase();
  const keywordAlert = config.accident_keywords.some((keyword) => normalizedAlert.includes(keyword));
  const alertActive = normalizedAlert === "ON" || keywordAlert;
  const isAccidentAlert = keywordAlert || reading.accident;

  ui.alertBanner.textContent = `Alert: ${normalizedAlert}`;
  ui.alertBanner.dataset.state = alertActive ? (isAccidentAlert ? "danger" : "warning") : "off";

  if (!alertActive) {
    ui.vehicleAlertWindow.hidden = true;
    ui.vehicleAlertWindow.dataset.state = "off";
    return;
  }

  ui.vehicleAlertWindow.hidden = false;
  ui.vehicleAlertWindow.dataset.state = isAccidentAlert ? "danger" : "warning";
  ui.vehicleAlertTitle.textContent = isAccidentAlert ? "ACCIDENT ALERT !!!" : "ALERT ACTIVE !!!";
  ui.vehicleAlertReason.textContent = reading.accident_reason.length
    ? reading.accident_reason.join(" | ")
    : `Alert state: ${normalizedAlert}`;
}

function appendLog(reading) {
  const item = document.createElement("li");
  item.className = "log-item";
  const strong = document.createElement("strong");
  strong.textContent = reading.raw_line;

  const meta = document.createElement("span");
  meta.textContent = `${formatTimestamp(reading.timestamp)} | source=${reading.source} | near=${zoneLabels[reading.near_zone] || "No Data"}`;

  item.appendChild(strong);
  item.appendChild(meta);

  ui.logList.prepend(item);
  while (ui.logList.children.length > 10) {
    ui.logList.removeChild(ui.logList.lastElementChild);
  }
}

function rebuildLogs(items) {
  ui.logList.innerHTML = "";
  items.forEach((item) => appendLog(item));
}

function createChart(canvasId, datasets, yAxisTitle) {
  const canvas = document.getElementById(canvasId);
  return new Chart(canvas, {
    type: "line",
    data: {
      labels: [],
      datasets,
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      animation: false,
      interaction: {
        mode: "index",
        intersect: false,
      },
      plugins: {
        legend: {
          labels: {
            color: "#ecf8ff",
            font: {
              family: "IBM Plex Mono",
            },
          },
        },
      },
      scales: {
        x: {
          ticks: {
            color: "#9cb5c7",
            maxRotation: 0,
          },
          grid: {
            color: "rgba(255, 255, 255, 0.06)",
          },
        },
        y: {
          ticks: {
            color: "#9cb5c7",
          },
          title: {
            display: true,
            text: yAxisTitle,
            color: "#9cb5c7",
          },
          grid: {
            color: "rgba(255, 255, 255, 0.06)",
          },
        },
      },
    },
  });
}

function ensureCharts() {
  if (typeof Chart === "undefined") {
    return;
  }

  if (!state.charts.vl53) {
    state.charts.vl53 = createChart(
      "vl53Chart",
      [
        {
          label: "Front (S1)",
          data: [],
          borderColor: "#6dd3ff",
          backgroundColor: "rgba(109, 211, 255, 0.14)",
          tension: 0.35,
          spanGaps: true,
        },
        {
          label: "Rear (S2)",
          data: [],
          borderColor: "#39d98a",
          backgroundColor: "rgba(57, 217, 138, 0.14)",
          tension: 0.35,
          spanGaps: true,
        },
      ],
      "Distance (cm)",
    );
  }

  if (!state.charts.adxl) {
    state.charts.adxl = createChart(
      "adxlChart",
      [
        {
          label: "AX",
          data: [],
          borderColor: "#ffd166",
          backgroundColor: "rgba(255, 209, 102, 0.12)",
          tension: 0.35,
          spanGaps: true,
        },
        {
          label: "dA",
          data: [],
          borderColor: "#ff5c5c",
          backgroundColor: "rgba(255, 92, 92, 0.12)",
          tension: 0.35,
          spanGaps: true,
        },
      ],
      "Acceleration",
    );
  }
}

function resetChart(chart) {
  chart.data.labels = [];
  chart.data.datasets.forEach((dataset) => {
    dataset.data = [];
  });
}

function rebuildCharts(items) {
  ensureCharts();
  if (!state.charts.vl53 || !state.charts.adxl) {
    return;
  }

  resetChart(state.charts.vl53);
  resetChart(state.charts.adxl);

  items.forEach((reading) => {
    pushChartPoint(reading, false);
  });

  state.charts.vl53.update("none");
  state.charts.adxl.update("none");
}

function trimChart(chart, limit) {
  while (chart.data.labels.length > limit) {
    chart.data.labels.shift();
    chart.data.datasets.forEach((dataset) => dataset.data.shift());
  }
}

function pushChartPoint(reading, update = true) {
  ensureCharts();
  if (!state.charts.vl53 || !state.charts.adxl) {
    return;
  }

  const label = formatTimestamp(reading.timestamp);
  const limit = config.default_history_points;

  state.charts.vl53.data.labels.push(label);
  state.charts.vl53.data.datasets[0].data.push(reading.s1);
  state.charts.vl53.data.datasets[1].data.push(reading.s2);
  trimChart(state.charts.vl53, limit);

  state.charts.adxl.data.labels.push(label);
  state.charts.adxl.data.datasets[0].data.push(reading.ax);
  state.charts.adxl.data.datasets[1].data.push(reading.da);
  trimChart(state.charts.adxl, limit);

  if (update) {
    state.charts.vl53.update("none");
    state.charts.adxl.update("none");
  }
}

function renderReading(reading, options = {}) {
  const { appendLiveLog = true, updateChart = true } = options;
  state.latestTimestamp = reading.timestamp;

  ui.lastUpdateText.textContent = formatTimestamp(reading.timestamp);
  ui.frontSensorText.textContent = formatDistance(reading.s1);
  ui.rearSensorText.textContent = formatDistance(reading.s2);
  ui.nearPill.textContent = `Near: ${formatDistance(reading.near)}`;
  updateVehicleAlert(reading);

  renderRadar(ui.frontRadar, reading.front_zone, reading.s1);
  renderRadar(ui.rearRadar, reading.rear_zone, reading.s2);

  ui.values.front.textContent = formatDistance(reading.s1);
  ui.meta.front.textContent = reading.s1 === null
    ? "S1 inactive"
    : `Zone: ${zoneLabels[reading.front_zone]}`;
  setCardTone(ui.cards.front, toneFromReadingZone(reading.front_zone));

  ui.values.rear.textContent = formatDistance(reading.s2);
  ui.meta.rear.textContent = reading.s2 === null
    ? "S2 inactive"
    : `Zone: ${zoneLabels[reading.rear_zone]}`;
  setCardTone(ui.cards.rear, toneFromReadingZone(reading.rear_zone));

  ui.values.ax.textContent = formatNumber(reading.ax);
  setCardTone(ui.cards.ax, reading.ax === null ? "neutral" : "safe");

  ui.values.da.textContent = formatNumber(reading.da);
  ui.meta.da.textContent = reading.da === null
    ? "Delta acceleration"
    : `Threshold: ${config.accident_delta_threshold.toFixed(3)}`;
  setCardTone(
    ui.cards.da,
    reading.da !== null && Math.abs(reading.da) >= config.accident_delta_threshold ? "danger" : "neutral",
  );

  ui.values.buzz.textContent = reading.buzz;
  setCardTone(
    ui.cards.buzz,
    ["CONTINUOUS", "FAST"].includes(reading.buzz) ? "danger" : reading.buzz === "SLOW" ? "warning" : "neutral",
  );

  ui.values.rgb.textContent = reading.rgb;
  setCardTone(
    ui.cards.rgb,
    reading.rgb.includes("RED") ? "danger" : reading.rgb.includes("YELLOW") ? "warning" : "safe",
  );

  ui.values.alert.textContent = reading.alert;
  ui.meta.alert.textContent = reading.accident
    ? reading.accident_reason.join(" | ")
    : "No accident condition";
  setCardTone(ui.cards.alert, reading.accident ? "danger" : reading.alert === "OFF" ? "safe" : "warning");

  if (appendLiveLog) {
    appendLog(reading);
  }

  if (updateChart) {
    pushChartPoint(reading);
  }
}

async function fetchHistory() {
  const response = await fetch(`/history?limit=${config.default_history_points}`);
  const payload = await response.json();

  rebuildLogs(payload.items);
  rebuildCharts(payload.items);

  if (payload.items.length > 0) {
    renderReading(payload.items[payload.items.length - 1], {
      appendLiveLog: false,
      updateChart: false,
    });
  }
}

async function fetchLatest() {
  const response = await fetch("/latest");
  const payload = await response.json();
  if (!payload.data) {
    return;
  }

  if (payload.data.timestamp === state.latestTimestamp) {
    return;
  }

  renderReading(payload.data);
}

function startPolling() {
  if (state.pollTimer) {
    return;
  }

  setConnectionMode("Polling");
  state.pollTimer = window.setInterval(() => {
    fetchLatest().catch(() => {
      setConnectionMode("Polling Retry");
    });
  }, config.poll_interval_ms);
}

function startWebSocket() {
  const protocol = window.location.protocol === "https:" ? "wss" : "ws";
  const socket = new WebSocket(`${protocol}://${window.location.host}/ws`);

  socket.addEventListener("open", () => {
    setConnectionMode("WebSocket");
    if (state.pollTimer) {
      window.clearInterval(state.pollTimer);
      state.pollTimer = null;
    }
  });

  socket.addEventListener("message", (event) => {
    const reading = JSON.parse(event.data);
    if (reading.timestamp === state.latestTimestamp) {
      return;
    }
    renderReading(reading);
  });

  socket.addEventListener("close", () => {
    if (!state.pollTimer) {
      startPolling();
    }
  });

  socket.addEventListener("error", () => {
    socket.close();
  });
}

async function initDashboard() {
  try {
    await fetchHistory();
    await fetchLatest();
  } catch (error) {
    setConnectionMode("Offline");
    return;
  }

  startWebSocket();
  startPolling();
}

initDashboard();
