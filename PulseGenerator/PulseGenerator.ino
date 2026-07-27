/*
 * XIAO ESP32S3 Pulse Generator
 * ----------------------------
 * Outputs 1 ms pulses on pin D10 at a selectable frequency (1–500 Hz).
 * Frequency and run duration (1–60 minutes) are set from a smartphone
 * over WiFi via an embedded web page with graphic sliders.
 *
 * Board:  Seeed XIAO ESP32S3
 * Pin:    D10 (GPIO9)
 *
 * How to use:
 *   1. Flash this sketch with the Arduino IDE (ESP32 board package,
 *      board = "XIAO_ESP32S3").
 *   2. On your phone, join WiFi network "XIAO-PulseGen"
 *      (password: pulse1234).
 *   3. Open http://192.168.4.1 in the phone browser.
 *   4. Set Frequency and Duration with the sliders, then tap Start.
 */

#include <WiFi.h>
#include <WebServer.h>
#include <esp_timer.h>

// -------- Hardware --------
static const int PULSE_PIN = D10;          // XIAO ESP32S3 D10 = GPIO9
static const uint32_t PULSE_WIDTH_US = 1000; // fixed 1 ms high time

// -------- SoftAP credentials --------
static const char* AP_SSID     = "XIAO-PulseGen";
static const char* AP_PASSWORD = "pulse1234";  // min 8 chars for WPA2

// -------- Limits --------
static const int FREQ_MIN_HZ   = 1;
static const int FREQ_MAX_HZ   = 500;
static const int DUR_MIN_MIN   = 1;
static const int DUR_MAX_MIN   = 60;

// -------- Runtime state (shared with timer ISR context) --------
volatile int      g_freqHz       = 10;
volatile int      g_durationMin  = 5;
volatile bool     g_running      = false;
volatile bool     g_pulseHigh    = false;
volatile uint32_t g_periodUs     = 100000;  // 1e6 / freq
volatile uint64_t g_endMs        = 0;       // millis() deadline

WebServer server(80);
esp_timer_handle_t pulseTimer = nullptr;

// Forward declarations
void IRAM_ATTR onPulseTimer(void* arg);
void scheduleNextEdge();
void startPulseTrain();
void stopPulseTrain();
void handleRoot();
void handleStatus();
void handleStart();
void handleStop();
void handleNotFound();

// ============================================================
// Pulse timing (esp_timer one-shots)
// ============================================================

void IRAM_ATTR onPulseTimer(void* /*arg*/) {
  if (!g_running) {
    digitalWrite(PULSE_PIN, LOW);
    g_pulseHigh = false;
    return;
  }

  // Stop when duration expires
  if ((uint64_t)millis() >= g_endMs) {
    digitalWrite(PULSE_PIN, LOW);
    g_pulseHigh = false;
    g_running = false;
    return;
  }

  if (g_pulseHigh) {
    // End of 1 ms high pulse → go low for the remainder of the period
    digitalWrite(PULSE_PIN, LOW);
    g_pulseHigh = false;

    uint32_t lowUs = (g_periodUs > PULSE_WIDTH_US)
                       ? (g_periodUs - PULSE_WIDTH_US)
                       : 1;
    esp_timer_start_once(pulseTimer, lowUs);
  } else {
    // Start next 1 ms high pulse
    digitalWrite(PULSE_PIN, HIGH);
    g_pulseHigh = true;
    esp_timer_start_once(pulseTimer, PULSE_WIDTH_US);
  }
}

void scheduleNextEdge() {
  // Kick off the first rising edge
  digitalWrite(PULSE_PIN, HIGH);
  g_pulseHigh = true;
  esp_timer_start_once(pulseTimer, PULSE_WIDTH_US);
}

void startPulseTrain() {
  if (g_running) {
    stopPulseTrain();
  }

  int freq = constrain(g_freqHz, FREQ_MIN_HZ, FREQ_MAX_HZ);
  int dur  = constrain(g_durationMin, DUR_MIN_MIN, DUR_MAX_MIN);

  g_freqHz      = freq;
  g_durationMin = dur;
  g_periodUs    = 1000000UL / (uint32_t)freq;
  g_endMs       = (uint64_t)millis() + (uint64_t)dur * 60000ULL;
  g_running     = true;

  scheduleNextEdge();
}

void stopPulseTrain() {
  g_running = false;
  esp_timer_stop(pulseTimer);
  digitalWrite(PULSE_PIN, LOW);
  g_pulseHigh = false;
}

// ============================================================
// Embedded smartphone UI
// ============================================================

static const char INDEX_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<meta name="apple-mobile-web-app-capable" content="yes">
<title>Pulse Generator</title>
<style>
  :root {
    --bg0: #0b1220;
    --bg1: #142033;
    --ink: #e8eef7;
    --muted: #93a4bd;
    --accent: #2ec4b6;
    --accent-dim: #1a8f85;
    --danger: #e35d6a;
    --danger-dim: #a33d48;
    --track: #24344d;
    --ok: #7ddea8;
  }
  * { box-sizing: border-box; -webkit-tap-highlight-color: transparent; }
  html, body {
    margin: 0; min-height: 100%;
    font-family: "Segoe UI", system-ui, -apple-system, sans-serif;
    color: var(--ink);
    background:
      radial-gradient(1200px 600px at 10% -10%, #1b3a4b 0%, transparent 55%),
      radial-gradient(900px 500px at 100% 0%, #24305a 0%, transparent 50%),
      linear-gradient(165deg, var(--bg0), var(--bg1));
  }
  .wrap {
    max-width: 440px;
    margin: 0 auto;
    padding: 28px 20px 40px;
  }
  h1 {
    margin: 0 0 6px;
    font-size: 1.55rem;
    font-weight: 650;
    letter-spacing: 0.02em;
  }
  .sub {
    margin: 0 0 28px;
    color: var(--muted);
    font-size: 0.95rem;
  }
  .panel {
    background: rgba(255,255,255,0.04);
    border: 1px solid rgba(255,255,255,0.08);
    border-radius: 18px;
    padding: 18px 16px 14px;
    margin-bottom: 16px;
  }
  .row {
    display: flex;
    justify-content: space-between;
    align-items: baseline;
    margin-bottom: 10px;
  }
  .label { font-size: 0.95rem; color: var(--muted); }
  .value {
    font-variant-numeric: tabular-nums;
    font-size: 1.35rem;
    font-weight: 650;
    color: var(--accent);
  }
  input[type=range] {
    -webkit-appearance: none;
    appearance: none;
    width: 100%;
    height: 36px;
    background: transparent;
    margin: 0;
  }
  input[type=range]::-webkit-slider-runnable-track {
    height: 10px;
    border-radius: 999px;
    background: linear-gradient(90deg, var(--accent-dim), var(--accent));
    box-shadow: inset 0 0 0 1px rgba(255,255,255,0.08);
  }
  input[type=range]::-moz-range-track {
    height: 10px;
    border-radius: 999px;
    background: linear-gradient(90deg, var(--accent-dim), var(--accent));
  }
  input[type=range]::-webkit-slider-thumb {
    -webkit-appearance: none;
    width: 28px; height: 28px;
    margin-top: -9px;
    border-radius: 50%;
    background: #fff;
    border: 3px solid var(--accent);
    box-shadow: 0 2px 10px rgba(0,0,0,0.35);
  }
  input[type=range]::-moz-range-thumb {
    width: 28px; height: 28px;
    border-radius: 50%;
    background: #fff;
    border: 3px solid var(--accent);
  }
  .hint {
    margin-top: 4px;
    font-size: 0.8rem;
    color: var(--muted);
  }
  .actions {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 12px;
    margin: 22px 0 18px;
  }
  button {
    appearance: none;
    border: 0;
    border-radius: 14px;
    padding: 16px 12px;
    font-size: 1.05rem;
    font-weight: 650;
    cursor: pointer;
  }
  #btnStart {
    background: linear-gradient(180deg, #3ad7c8, var(--accent-dim));
    color: #062a27;
  }
  #btnStop {
    background: linear-gradient(180deg, #f07a85, var(--danger-dim));
    color: #2a0a0e;
  }
  button:active { transform: scale(0.98); opacity: 0.92; }
  button:disabled { opacity: 0.45; cursor: default; transform: none; }
  .status {
    display: grid;
    gap: 8px;
    padding: 14px 16px;
    border-radius: 14px;
    background: rgba(0,0,0,0.22);
    border: 1px solid rgba(255,255,255,0.07);
    font-size: 0.95rem;
  }
  .status .k { color: var(--muted); }
  .status .v { font-variant-numeric: tabular-nums; font-weight: 600; }
  .pill {
    display: inline-block;
    padding: 3px 10px;
    border-radius: 999px;
    font-size: 0.85rem;
    font-weight: 700;
    letter-spacing: 0.04em;
  }
  .pill.on  { background: rgba(125,222,168,0.18); color: var(--ok); }
  .pill.off { background: rgba(147,164,189,0.16); color: var(--muted); }
  .foot {
    margin-top: 18px;
    text-align: center;
    color: var(--muted);
    font-size: 0.8rem;
  }
</style>
</head>
<body>
  <div class="wrap">
    <h1>Pulse Generator</h1>
    <p class="sub">XIAO ESP32S3 · 1&nbsp;ms pulses on D10</p>

    <div class="panel">
      <div class="row">
        <span class="label">Frequency</span>
        <span class="value"><span id="freqVal">10</span> Hz</span>
      </div>
      <input id="freq" type="range" min="1" max="500" step="1" value="10">
      <div class="hint">1 – 500 Hz &nbsp;·&nbsp; pulse width fixed at 1 ms</div>
    </div>

    <div class="panel">
      <div class="row">
        <span class="label">Duration</span>
        <span class="value"><span id="durVal">5</span> min</span>
      </div>
      <input id="dur" type="range" min="1" max="60" step="1" value="5">
      <div class="hint">1 – 60 minutes</div>
    </div>

    <div class="actions">
      <button id="btnStart" type="button">Start</button>
      <button id="btnStop" type="button">Stop</button>
    </div>

    <div class="status">
      <div><span class="k">State</span> · <span id="statePill" class="pill off">IDLE</span></div>
      <div><span class="k">Output</span> · <span class="v" id="outFreq">—</span></div>
      <div><span class="k">Remaining</span> · <span class="v" id="remain">—</span></div>
    </div>

    <p class="foot">Connect to Wi‑Fi <b>XIAO-PulseGen</b> · open 192.168.4.1</p>
  </div>

<script>
  const freq = document.getElementById('freq');
  const dur  = document.getElementById('dur');
  const freqVal = document.getElementById('freqVal');
  const durVal  = document.getElementById('durVal');
  const btnStart = document.getElementById('btnStart');
  const btnStop  = document.getElementById('btnStop');
  const statePill = document.getElementById('statePill');
  const outFreq = document.getElementById('outFreq');
  const remain  = document.getElementById('remain');

  function fmtRemain(sec) {
    if (sec == null || sec < 0) return '—';
    const m = Math.floor(sec / 60);
    const s = Math.floor(sec % 60);
    return m + 'm ' + String(s).padStart(2, '0') + 's';
  }

  freq.addEventListener('input', () => { freqVal.textContent = freq.value; });
  dur.addEventListener('input',  () => { durVal.textContent  = dur.value; });

  btnStart.addEventListener('click', async () => {
    btnStart.disabled = true;
    try {
      const q = '/start?freq=' + encodeURIComponent(freq.value)
               + '&duration=' + encodeURIComponent(dur.value);
      await fetch(q);
      await refresh();
    } finally {
      btnStart.disabled = false;
    }
  });

  btnStop.addEventListener('click', async () => {
    btnStop.disabled = true;
    try {
      await fetch('/stop');
      await refresh();
    } finally {
      btnStop.disabled = false;
    }
  });

  async function refresh() {
    try {
      const r = await fetch('/status');
      const j = await r.json();
      freq.value = j.freq;
      dur.value  = j.duration;
      freqVal.textContent = j.freq;
      durVal.textContent  = j.duration;

      if (j.running) {
        statePill.textContent = 'RUNNING';
        statePill.className = 'pill on';
        outFreq.textContent = j.freq + ' Hz · 1 ms';
        remain.textContent  = fmtRemain(j.remainingSec);
        freq.disabled = true;
        dur.disabled  = true;
      } else {
        statePill.textContent = 'IDLE';
        statePill.className = 'pill off';
        outFreq.textContent = 'off';
        remain.textContent  = '—';
        freq.disabled = false;
        dur.disabled  = false;
      }
    } catch (e) {
      statePill.textContent = 'OFFLINE';
      statePill.className = 'pill off';
    }
  }

  refresh();
  setInterval(refresh, 1000);
</script>
</body>
</html>
)HTML';

// ============================================================
// HTTP handlers
// ============================================================

void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleStatus() {
  int remainingSec = 0;
  if (g_running) {
    uint64_t now = (uint64_t)millis();
    if (now < g_endMs) {
      remainingSec = (int)((g_endMs - now) / 1000ULL);
    }
  }

  String json = "{";
  json += "\"running\":";
  json += g_running ? "true" : "false";
  json += ",\"freq\":";
  json += String((int)g_freqHz);
  json += ",\"duration\":";
  json += String((int)g_durationMin);
  json += ",\"pulseUs\":";
  json += String((unsigned long)PULSE_WIDTH_US);
  json += ",\"remainingSec\":";
  json += String(remainingSec);
  json += "}";

  server.send(200, "application/json", json);
}

void handleStart() {
  int freq = g_freqHz;
  int dur  = g_durationMin;

  if (server.hasArg("freq")) {
    freq = server.arg("freq").toInt();
  }
  if (server.hasArg("duration")) {
    dur = server.arg("duration").toInt();
  }

  freq = constrain(freq, FREQ_MIN_HZ, FREQ_MAX_HZ);
  dur  = constrain(dur, DUR_MIN_MIN, DUR_MAX_MIN);

  g_freqHz      = freq;
  g_durationMin = dur;
  startPulseTrain();

  server.send(200, "application/json", "{\"ok\":true}");
}

void handleStop() {
  stopPulseTrain();
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

// ============================================================
// Setup / loop
// ============================================================

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(PULSE_PIN, OUTPUT);
  digitalWrite(PULSE_PIN, LOW);

  // High-resolution one-shot timer for pulse edges
  const esp_timer_create_args_t timerArgs = {
    .callback = &onPulseTimer,
    .arg = nullptr,
    .dispatch_method = ESP_TIMER_TASK,
    .name = "pulse",
    .skip_unhandled_events = true
  };
  esp_timer_create(&timerArgs, &pulseTimer);

  // SoftAP — phone joins this network
  WiFi.mode(WIFI_AP);
  bool apOk = WiFi.softAP(AP_SSID, AP_PASSWORD);
  IPAddress ip = WiFi.softAPIP();

  Serial.println();
  Serial.println(F("=== XIAO ESP32S3 Pulse Generator ==="));
  Serial.printf("SoftAP %s  (%s)\n", apOk ? "OK" : "FAILED", AP_SSID);
  Serial.printf("Password: %s\n", AP_PASSWORD);
  Serial.printf("Open http://%s on your phone\n", ip.toString().c_str());
  Serial.printf("Pulse pin: D10  |  width: %lu us\n", (unsigned long)PULSE_WIDTH_US);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/start", HTTP_GET, handleStart);
  server.on("/stop", HTTP_GET, handleStop);
  server.onNotFound(handleNotFound);
  server.begin();
}

void loop() {
  server.handleClient();

  // Safety: if duration elapsed but timer missed the stop, force idle
  if (g_running && (uint64_t)millis() >= g_endMs) {
    stopPulseTrain();
  }
}
