/*
  XIAO ESP32-C3 + Seeed Expansion Board
  Piezo vibration monitor with OLED + phone dashboard + Wi-Fi setup wizard.

  Hardware
    Piezo on D1 (A1 / GPIO3)  — peak-to-peak analog
    OLED 128x64 SSD1306 on I2C (SDA=D4, SCL=D5)
    Expansion USER button is also D1. Hold it at boot to force the Wi-Fi wizard.

  Libraries (Library Manager)
    U8g2  by oliver

  Board
    Seeed XIAO ESP32C3  (or "XIAO_ESP32C3")
    USB CDC On Boot: Enabled

  This is a single-file sketch. Copy only this .ino into Arduino IDE.
*/

#include <Arduino.h>
#include <math.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>

// Phone dashboard + Wi-Fi wizard HTML (keep these above setup()/loop()).
static const char DASHBOARD_HTML[] PROGMEM = R"html(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<meta name="apple-mobile-web-app-capable" content="yes">
<title>Vibe Monitor</title>
<style>
  :root {
    --bg: #0b1020;
    --card: #151b2e;
    --line: #2a3350;
    --text: #eef2ff;
    --muted: #9aa6c3;
    --quiet: #3dd68c;
    --warn: #f5c14a;
    --hot: #ff6b6b;
    --accent: #6ea8fe;
  }
  * { box-sizing: border-box; }
  body {
    margin: 0;
    min-height: 100vh;
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
    background: radial-gradient(circle at top, #182244, var(--bg) 55%);
    color: var(--text);
  }
  main { max-width: 440px; margin: 0 auto; padding: 20px 16px 32px; }
  h1 { font-size: 1.15rem; margin: 0 0 4px; letter-spacing: .04em; }
  .sub { color: var(--muted); font-size: .85rem; margin-bottom: 16px; }
  .card {
    background: var(--card);
    border: 1px solid var(--line);
    border-radius: 16px;
    padding: 16px;
    margin-bottom: 12px;
  }
  .amp {
    display: flex;
    align-items: baseline;
    justify-content: space-between;
  }
  .amp-num { font-size: 4.2rem; font-weight: 700; line-height: 1; }
  .amp-unit { color: var(--muted); font-size: 1.1rem; }
  .pill {
    display: inline-block;
    padding: 6px 10px;
    border-radius: 999px;
    font-size: .75rem;
    font-weight: 700;
    letter-spacing: .06em;
    background: #1d2a22;
    color: var(--quiet);
  }
  .pill.hot { background: #3a1d22; color: var(--hot); }
  .bar {
    height: 16px;
    border-radius: 999px;
    background: #0e1424;
    overflow: hidden;
    margin: 14px 0 8px;
  }
  .bar > span {
    display: block;
    height: 100%;
    width: 0;
    border-radius: inherit;
    background: linear-gradient(90deg, var(--quiet), var(--warn), var(--hot));
    transition: width .15s linear;
  }
  .meta { display: flex; justify-content: space-between; color: var(--muted); font-size: .8rem; }
  canvas { width: 100%; height: 84px; display: block; }
  label { display: block; font-size: .8rem; color: var(--muted); margin-bottom: 8px; }
  input[type=range] { width: 100%; }
  button, .btn {
    appearance: none;
    border: 0;
    border-radius: 12px;
    padding: 12px 14px;
    width: 100%;
    font-weight: 700;
    font-size: .95rem;
    background: var(--accent);
    color: #081018;
  }
  .ghost { background: #222a42; color: var(--text); margin-top: 8px; }
  .row { display: flex; justify-content: space-between; gap: 8px; font-size: .8rem; color: var(--muted); }
</style>
</head>
<body>
<main>
  <h1>PIEZO VIBRATION</h1>
  <div class="sub" id="net">Connecting…</div>
  <section class="card">
    <div class="amp">
      <div><span class="amp-num" id="amp">0</span><span class="amp-unit"> %</span></div>
      <span class="pill" id="pill">QUIET</span>
    </div>
    <div class="bar"><span id="fill"></span></div>
    <div class="meta">
      <span>Peak <b id="peak">0</b>%</span>
      <span>Raw <b id="raw">0</b></span>
    </div>
  </section>
  <section class="card">
    <label>Recent level</label>
    <canvas id="spark" width="400" height="84"></canvas>
  </section>
  <section class="card">
    <label>Sensitivity <span id="sensVal">50</span></label>
    <input id="sens" type="range" min="10" max="100" value="50">
  </section>
  <section class="card">
    <label>Average window <span id="avgVal">1.0 s</span></label>
    <input id="avgWin" type="range" min="0" max="10" step="0.1" value="1">
  </section>
  <div class="row">
    <span id="ip"></span>
    <span id="rssi"></span>
  </div>
  <p><a class="btn ghost" href="/wifi">Change Wi-Fi</a></p>
</main>
<script>
const hist = Array(48).fill(0);
const canvas = document.getElementById("spark");
const ctx = canvas.getContext("2d");

function colorFor(v) {
  if (v >= 50) return getComputedStyle(document.documentElement).getPropertyValue("--hot").trim();
  if (v >= 15) return getComputedStyle(document.documentElement).getPropertyValue("--warn").trim();
  return getComputedStyle(document.documentElement).getPropertyValue("--quiet").trim();
}

function draw() {
  const w = canvas.width, h = canvas.height;
  ctx.clearRect(0, 0, w, h);
  ctx.strokeStyle = "#2a3350";
  ctx.beginPath();
  ctx.moveTo(0, h - 8);
  ctx.lineTo(w, h - 8);
  ctx.stroke();
  ctx.beginPath();
  hist.forEach((v, i) => {
    const x = (i / (hist.length - 1)) * w;
    const y = (h - 8) - (v / 100) * (h - 16);
    if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
  });
  ctx.strokeStyle = "#6ea8fe";
  ctx.lineWidth = 2;
  ctx.stroke();
}

function fmtWin(v) {
  const n = Number(v);
  if (n <= 0) return "0 s (instant)";
  return n.toFixed(1) + " s";
}

function apply(d) {
  document.getElementById("amp").textContent = d.amplitude;
  document.getElementById("peak").textContent = d.peak;
  document.getElementById("raw").textContent = d.raw;
  document.getElementById("fill").style.width = d.amplitude + "%";
  document.getElementById("amp").style.color = colorFor(d.amplitude);
  const pill = document.getElementById("pill");
  pill.textContent = d.detected ? "DETECTED" : "QUIET";
  pill.className = "pill" + (d.detected ? " hot" : "");
  document.getElementById("net").textContent = (d.ssid || "Wi-Fi") + (d.mdns ? "  ·  " + d.mdns : "");
  document.getElementById("ip").textContent = d.ip || "";
  document.getElementById("rssi").textContent = (typeof d.rssi === "number") ? (d.rssi + " dBm") : "";
  if (typeof d.sensitivity === "number" && document.activeElement !== document.getElementById("sens")) {
    document.getElementById("sens").value = d.sensitivity;
    document.getElementById("sensVal").textContent = d.sensitivity;
  }
  if (typeof d.avgWindow === "number" && document.activeElement !== document.getElementById("avgWin")) {
    document.getElementById("avgWin").value = d.avgWindow;
    document.getElementById("avgVal").textContent = fmtWin(d.avgWindow);
  }
  hist.push(d.amplitude);
  if (hist.length > 48) hist.shift();
  draw();
}

async function tick() {
  try {
    const r = await fetch("/api/status", { cache: "no-store" });
    apply(await r.json());
  } catch (e) {
    document.getElementById("net").textContent = "Lost connection — retrying";
  }
}

document.getElementById("sens").addEventListener("input", (ev) => {
  document.getElementById("sensVal").textContent = ev.target.value;
});
document.getElementById("sens").addEventListener("change", async (ev) => {
  await fetch("/api/sensitivity?value=" + ev.target.value, { method: "POST" });
});
document.getElementById("avgWin").addEventListener("input", (ev) => {
  document.getElementById("avgVal").textContent = fmtWin(ev.target.value);
});
document.getElementById("avgWin").addEventListener("change", async (ev) => {
  await fetch("/api/avgwindow?value=" + ev.target.value, { method: "POST" });
});

setInterval(tick, 250);
tick();
</script>
</body>
</html>
)html";

static const char WIZARD_HTML[] PROGMEM = R"html(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<title>Vibe Monitor Setup</title>
<style>
  body {
    margin: 0;
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
    background: #0b1020;
    color: #eef2ff;
  }
  main { max-width: 440px; margin: 0 auto; padding: 22px 16px; }
  h1 { font-size: 1.3rem; margin: 0 0 8px; }
  p { color: #9aa6c3; line-height: 1.45; }
  .card {
    background: #151b2e;
    border: 1px solid #2a3350;
    border-radius: 16px;
    padding: 14px;
    margin: 14px 0;
  }
  .net {
    display: flex;
    justify-content: space-between;
    padding: 12px 4px;
    border-bottom: 1px solid #2a3350;
    cursor: pointer;
  }
  .net:last-child { border-bottom: 0; }
  .ssid { font-weight: 700; }
  .rssi { color: #9aa6c3; font-size: .8rem; }
  input, button {
    width: 100%;
    border-radius: 12px;
    border: 1px solid #2a3350;
    padding: 12px;
    font-size: 1rem;
    margin-top: 8px;
  }
  input { background: #0e1424; color: #eef2ff; }
  button { border: 0; background: #6ea8fe; color: #081018; font-weight: 700; }
  .ghost { background: #222a42; color: #eef2ff; }
  .err { color: #ff6b6b; }
  .ok { color: #3dd68c; }
</style>
</head>
<body>
<main>
  <h1>Wi-Fi setup</h1>
  <p>Pick your network, enter the password, then open the live vibration page on your phone.</p>
  <div class="card" id="list">Scanning…</div>
  <form class="card" method="POST" action="/wifi/save">
    <label>Network</label>
    <input name="ssid" id="ssid" autocomplete="off" required>
    <label>Password</label>
    <input name="pass" id="pass" type="password">
    <button type="submit">Connect</button>
  </form>
  <button class="ghost" onclick="scan()">Rescan</button>
  <p id="msg"></p>
</main>
<script>
function bars(rssi) {
  if (rssi >= -55) return "Strong";
  if (rssi >= -70) return "OK";
  return "Weak";
}
async function scan() {
  const list = document.getElementById("list");
  list.textContent = "Scanning…";
  try {
    const r = await fetch("/api/scan", { cache: "no-store" });
    const nets = await r.json();
    if (!nets.length) { list.textContent = "No networks found. Rescan."; return; }
    list.innerHTML = "";
    nets.forEach(n => {
      const row = document.createElement("div");
      row.className = "net";
      row.innerHTML = '<span class="ssid"></span><span class="rssi"></span>';
      row.querySelector(".ssid").textContent = n.ssid + (n.secure ? "  🔒" : "");
      row.querySelector(".rssi").textContent = bars(n.rssi);
      row.onclick = () => { document.getElementById("ssid").value = n.ssid; };
      list.appendChild(row);
    });
  } catch (e) {
    list.textContent = "Scan failed. Rescan.";
  }
}
const params = new URLSearchParams(location.search);
if (params.get("err")) {
  document.getElementById("msg").className = "err";
  document.getElementById("msg").textContent = params.get("err");
}
scan();
</script>
</body>
</html>
)html";

static const char CONNECTING_HTML[] PROGMEM = R"html(
<!DOCTYPE html>
<html><head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta http-equiv="refresh" content="4">
<title>Connecting</title>
<style>
body{font-family:-apple-system,sans-serif;background:#0b1020;color:#eef2ff;padding:24px}
</style>
</head>
<body>
<h1>Connecting…</h1>
<p>The monitor is joining your Wi-Fi. If this page stops loading, leave the setup network and open the IP shown on the OLED, or try <b>http://vibemonitor.local</b>.</p>
</body></html>
)html";

// ---- pins (XIAO ESP32-C3) ----
#ifndef D1
#define D1 3
#endif
#ifndef D4
#define D4 6
#endif
#ifndef D5
#define D5 7
#endif

const int PIEZO_PIN = D1;   // GPIO3, ADC1_CH3
const int PIN_SDA = D4;     // GPIO6
const int PIN_SCL = D5;     // GPIO7

const char* AP_NAME = "VibeMonitor";
const char* MDNS_NAME = "vibemonitor";

const uint32_t WIFI_CONNECT_MS = 20000;
const uint32_t SAMPLE_PERIOD_MS = 20;
const uint32_t OLED_PERIOD_MS = 120;
const int SAMPLE_COUNT = 80;
const int AVG_MAX_SAMPLES = 500;  // 10 s at 20 ms

enum RunMode : uint8_t { MODE_PORTAL, MODE_CONNECTING, MODE_RUN };

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
WebServer server(80);
DNSServer dnsServer;
Preferences prefs;

RunMode mode = MODE_PORTAL;
String wifiSsid;
String wifiPass;
uint32_t connectStarted = 0;
bool mdnsStarted = false;
bool serverStarted = false;

int sensitivity = 50;     // 10..100, higher = more sensitive
float avgWindowSec = 1.0f;  // 0..10 s moving average (0 = instant)
int amplitude = 0;        // 0..100 relative (after averaging)
int peakAmplitude = 0;
int lastRawP2P = 0;
bool detected = false;
uint32_t peakStamp = 0;

float noiseFloor = 40.0f;
float recentMax = 220.0f;
float avgBuf[AVG_MAX_SAMPLES];
uint16_t avgHead = 0;
uint16_t avgCount = 0;

uint32_t lastSample = 0;
uint32_t lastOled = 0;
uint32_t detectHoldUntil = 0;

void drawCentered(int y, const char* text);
void showBootScreen();
void showStatus(const char* a, const char* b = "", const char* c = "");
void drawPortalScreen();
void drawConnectingScreen();
void drawRunScreen();
void sampleVibration();
void startPortal(const char* err = nullptr);
void beginConnect();
void finishConnect();
void setupServer();
void ensureServerStarted();
void prepareWifiRadio();
void handleRoot();
void handleWizard();
void handleWifiSave();
void handleApiStatus();
void handleApiScan();
void handleApiSensitivity();
void handleApiAvgWindow();
void handleCaptive();
bool bootButtonHeld();
float averagedAmplitude(float instant);

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\nVibeMonitor starting");

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  pinMode(PIEZO_PIN, INPUT);

  Wire.begin(PIN_SDA, PIN_SCL);
  u8g2.begin();
  u8g2.setFlipMode(1);
  showBootScreen();
  delay(300);

  showStatus("Loading", "settings...");
  prefs.begin("vibe", false);
  wifiSsid = prefs.getString("ssid", "");
  wifiPass = prefs.getString("pass", "");
  sensitivity = constrain(prefs.getInt("sens", 50), 10, 100);
  avgWindowSec = prefs.getFloat("avgWin", 1.0f);
  if (avgWindowSec < 0.0f) avgWindowSec = 0.0f;
  if (avgWindowSec > 10.0f) avgWindowSec = 10.0f;

  setupServer();  // routes only; listen after Wi-Fi is up

  const bool forceWizard = bootButtonHeld();
  if (forceWizard) {
    Serial.println("USER button held — opening Wi-Fi wizard");
  }

  showStatus("Starting", "Wi-Fi radio", "please wait");
  prepareWifiRadio();

  if (forceWizard || wifiSsid.isEmpty()) {
    startPortal();
  } else {
    beginConnect();
  }
}

void loop() {
  server.handleClient();
  if (mode == MODE_PORTAL) {
    dnsServer.processNextRequest();
  }

  if (mode == MODE_CONNECTING) {
    if (WiFi.status() == WL_CONNECTED) {
      finishConnect();
    } else if (millis() - connectStarted > WIFI_CONNECT_MS) {
      Serial.println("Wi-Fi connect timed out");
      startPortal("Could not join that network. Check the password and try again.");
    }
  }

  if (mode == MODE_RUN && WiFi.status() != WL_CONNECTED) {
    static uint32_t lastTry = 0;
    if (millis() - lastTry > 8000) {
      lastTry = millis();
      Serial.println("Wi-Fi dropped — reconnecting");
      WiFi.reconnect();
    }
  }

  if (millis() - lastSample >= SAMPLE_PERIOD_MS) {
    lastSample = millis();
    sampleVibration();
  }

  if (millis() - lastOled >= OLED_PERIOD_MS) {
    lastOled = millis();
    if (mode == MODE_PORTAL) drawPortalScreen();
    else if (mode == MODE_CONNECTING) drawConnectingScreen();
    else drawRunScreen();
  }

  if (Serial.available()) {
    const char c = Serial.read();
    if (c == 'w' || c == 'W') {
      startPortal();
    } else if (c == 'c' || c == 'C') {
      prefs.remove("ssid");
      prefs.remove("pass");
      Serial.println("Cleared saved Wi-Fi");
    }
  }
}

bool bootButtonHeld() {
  // Expansion USER button shares D1 and pulls it to GND when pressed.
  pinMode(PIEZO_PIN, INPUT_PULLUP);
  delay(20);
  bool held = true;
  for (int i = 0; i < 8; i++) {
    if (digitalRead(PIEZO_PIN) != LOW) held = false;
    delay(10);
  }
  pinMode(PIEZO_PIN, INPUT);
  return held;
}

float averagedAmplitude(float instant) {
  avgBuf[avgHead] = instant;
  avgHead++;
  if (avgHead >= AVG_MAX_SAMPLES) avgHead = 0;
  if (avgCount < AVG_MAX_SAMPLES) avgCount++;

  int n = (int)lroundf(avgWindowSec * (1000.0f / (float)SAMPLE_PERIOD_MS));
  if (n <= 0) return instant;
  if (n > AVG_MAX_SAMPLES) n = AVG_MAX_SAMPLES;
  if (n > (int)avgCount) n = avgCount;
  if (n <= 0) return instant;

  float sum = 0.0f;
  int idx = (int)avgHead;
  for (int i = 0; i < n; i++) {
    idx--;
    if (idx < 0) idx += AVG_MAX_SAMPLES;
    sum += avgBuf[idx];
  }
  return sum / (float)n;
}

void sampleVibration() {
  int mn = 4095;
  int mx = 0;
  for (int i = 0; i < SAMPLE_COUNT; i++) {
    const int v = analogRead(PIEZO_PIN);
    if (v < mn) mn = v;
    if (v > mx) mx = v;
    delayMicroseconds(120);
  }

  lastRawP2P = mx - mn;

  // Slow-rising noise floor so idle chatter is ignored.
  if (lastRawP2P < noiseFloor) {
    noiseFloor = 0.85f * noiseFloor + 0.15f * lastRawP2P;
  } else {
    noiseFloor = 0.995f * noiseFloor + 0.005f * lastRawP2P;
  }
  if (noiseFloor < 12.0f) noiseFloor = 12.0f;

  const float excess = fmaxf(0.0f, (float)lastRawP2P - noiseFloor);
  recentMax = fmaxf(recentMax * 0.997f, fmaxf(180.0f, (float)lastRawP2P));
  const float gain = sensitivity / 50.0f;
  const float scale = fmaxf(160.0f, recentMax * 0.80f);
  const float instant = constrain(excess * gain * 100.0f / scale, 0.0f, 100.0f);
  amplitude = constrain((int)lroundf(averagedAmplitude(instant)), 0, 100);

  const int detectAt = map(sensitivity, 10, 100, 22, 6);
  if (amplitude >= detectAt) {
    detected = true;
    detectHoldUntil = millis() + 700;
  } else if (millis() > detectHoldUntil) {
    detected = false;
  }

  if (amplitude >= peakAmplitude) {
    peakAmplitude = amplitude;
    peakStamp = millis();
  } else if (millis() - peakStamp > 2000) {
    peakAmplitude = (int)fmaxf(0.0f, peakAmplitude * 0.96f);
  }
}

void showBootScreen() {
  showStatus("Vibe Monitor", "XIAO ESP32-C3", "starting...");
}

void showStatus(const char* a, const char* b, const char* c) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tf);
  if (a && a[0]) drawCentered(18, a);
  u8g2.setFont(u8g2_font_5x8_tf);
  if (b && b[0]) drawCentered(40, b);
  if (c && c[0]) drawCentered(52, c);
  u8g2.sendBuffer();
}

void drawCentered(int y, const char* text) {
  const int w = u8g2.getStrWidth(text);
  u8g2.drawStr((128 - w) / 2, y, text);
}

void drawPortalScreen() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tf);
  drawCentered(12, "Wi-Fi setup");
  u8g2.setFont(u8g2_font_5x8_tf);
  drawCentered(28, "Phone: join AP");
  u8g2.setFont(u8g2_font_6x12_tf);
  drawCentered(42, AP_NAME);
  u8g2.setFont(u8g2_font_5x8_tf);
  drawCentered(56, "then open 192.168.4.1");
  u8g2.sendBuffer();
}

void drawConnectingScreen() {
  char line[24];
  snprintf(line, sizeof(line), "Joining %s", wifiSsid.c_str());
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tf);
  drawCentered(20, "Connecting");
  u8g2.setFont(u8g2_font_5x8_tf);
  u8g2.drawStr(0, 40, line);
  const int dots = (millis() / 400) % 4;
  char wait[] = "....";
  wait[dots] = '\0';
  u8g2.drawStr(0, 56, wait);
  u8g2.sendBuffer();
}

void drawRunScreen() {
  char num[8];
  snprintf(num, sizeof(num), "%d", amplitude);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.drawStr(0, 10, "VIBRATION");
  u8g2.setFont(u8g2_font_5x8_tf);
  u8g2.drawStr(detected ? 78 : 92, 10, detected ? "DETECT" : "quiet");

  u8g2.setFont(u8g2_font_logisoso24_tn);
  const int nw = u8g2.getStrWidth(num);
  u8g2.drawStr(4, 40, num);
  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.drawStr(4 + nw + 4, 36, "%");

  const int barW = map(amplitude, 0, 100, 0, 124);
  u8g2.drawFrame(0, 46, 128, 8);
  if (barW > 0) u8g2.drawBox(2, 48, barW, 4);

  u8g2.setFont(u8g2_font_5x8_tf);
  if (WiFi.status() == WL_CONNECTED) {
    u8g2.drawStr(0, 63, WiFi.localIP().toString().c_str());
  } else {
    u8g2.drawStr(0, 63, "Wi-Fi lost");
  }
  u8g2.sendBuffer();
}

void setupServer() {
  server.on("/", handleRoot);
  server.on("/wifi", handleWizard);
  server.on("/wifi/save", HTTP_POST, handleWifiSave);
  server.on("/wifi/save", HTTP_GET, []() {
    server.send_P(200, "text/html", CONNECTING_HTML);
  });
  server.on("/api/status", handleApiStatus);
  server.on("/api/scan", handleApiScan);
  server.on("/api/sensitivity", HTTP_POST, handleApiSensitivity);
  server.on("/api/avgwindow", HTTP_POST, handleApiAvgWindow);

  // Captive-portal OS probes
  server.on("/generate_204", handleCaptive);
  server.on("/gen_204", handleCaptive);
  server.on("/hotspot-detect.html", handleCaptive);
  server.on("/library/test/success.html", handleCaptive);
  server.on("/connecttest.txt", handleCaptive);
  server.on("/ncsi.txt", handleCaptive);
  server.on("/canonical.html", handleCaptive);
  server.on("/success.txt", handleCaptive);
  server.on("/fwlink/", handleCaptive);
  server.onNotFound(handleCaptive);
  // Do not server.begin() here. ESP32 Arduino 3.x can hang if the
  // listener starts before the Wi-Fi radio is up.
}

void ensureServerStarted() {
  if (serverStarted) return;
  server.begin();
  serverStarted = true;
}

void prepareWifiRadio() {
  WiFi.persistent(false);
  WiFi.setSleep(false);
  WiFi.mode(WIFI_OFF);
  delay(200);
}

void handleCaptive() {
  if (mode == MODE_PORTAL) {
    server.sendHeader("Location", "http://192.168.4.1/wifi", true);
    server.send(302, "text/plain", "");
    return;
  }
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

void handleRoot() {
  if (mode == MODE_PORTAL) {
    handleWizard();
    return;
  }
  if (mode == MODE_CONNECTING) {
    server.send_P(200, "text/html", CONNECTING_HTML);
    return;
  }
  server.send_P(200, "text/html", DASHBOARD_HTML);
}

void handleWizard() {
  server.send_P(200, "text/html", WIZARD_HTML);
}

void handleWifiSave() {
  const String ssid = server.arg("ssid");
  const String pass = server.arg("pass");
  if (ssid.isEmpty()) {
    server.sendHeader("Location", "/wifi?err=Network%20name%20is%20required", true);
    server.send(302, "text/plain", "");
    return;
  }

  wifiSsid = ssid;
  wifiPass = pass;
  prefs.putString("ssid", wifiSsid);
  prefs.putString("pass", wifiPass);
  server.send_P(200, "text/html", CONNECTING_HTML);
  delay(200);
  beginConnect();
}

String jsonEscape(const String& in) {
  String out;
  out.reserve(in.length());
  for (size_t i = 0; i < in.length(); i++) {
    const char c = in[i];
    if (c == '\\' || c == '"') out += '\\';
    if (c >= 32) out += c;
  }
  return out;
}

void handleApiStatus() {
  String json = "{";
  json += "\"amplitude\":" + String(amplitude);
  json += ",\"peak\":" + String(peakAmplitude);
  json += ",\"raw\":" + String(lastRawP2P);
  json += ",\"detected\":";
  json += detected ? "true" : "false";
  json += ",\"sensitivity\":" + String(sensitivity);
  json += ",\"avgWindow\":" + String(avgWindowSec, 1);
  json += ",\"ssid\":\"" + jsonEscape(WiFi.SSID()) + "\"";
  json += ",\"ip\":\"" + WiFi.localIP().toString() + "\"";
  json += ",\"mdns\":\"http://";
  json += MDNS_NAME;
  json += ".local\"";
  json += ",\"rssi\":" + String(WiFi.RSSI());
  json += "}";
  server.send(200, "application/json", json);
}

void handleApiScan() {
  const int n = WiFi.scanNetworks();
  String json = "[";
  for (int i = 0; i < n; i++) {
    if (i) json += ",";
    json += "{\"ssid\":\"" + jsonEscape(WiFi.SSID(i)) + "\",\"rssi\":" + String(WiFi.RSSI(i));
    json += ",\"secure\":";
    json += (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "false" : "true";
    json += "}";
  }
  json += "]";
  WiFi.scanDelete();
  server.send(200, "application/json", json);
}

void handleApiSensitivity() {
  if (!server.hasArg("value")) {
    server.send(400, "text/plain", "missing value");
    return;
  }
  sensitivity = constrain(server.arg("value").toInt(), 10, 100);
  prefs.putInt("sens", sensitivity);
  server.send(200, "text/plain", "ok");
}

void handleApiAvgWindow() {
  if (!server.hasArg("value")) {
    server.send(400, "text/plain", "missing value");
    return;
  }
  float v = server.arg("value").toFloat();
  if (v < 0.0f) v = 0.0f;
  if (v > 10.0f) v = 10.0f;
  avgWindowSec = roundf(v * 10.0f) / 10.0f;
  prefs.putFloat("avgWin", avgWindowSec);
  server.send(200, "text/plain", "ok");
}

void startPortal(const char* err) {
  showStatus("Wi-Fi setup", "starting AP", AP_NAME);

  dnsServer.stop();
  WiFi.disconnect(false, false);
  delay(50);
  WiFi.mode(WIFI_OFF);
  delay(200);
  WiFi.mode(WIFI_AP);
  delay(200);
  // Channel 6 is more stable for ESP32-C3 SoftAP than channel 1.
  WiFi.softAP(AP_NAME, nullptr, 6);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  delay(150);
  dnsServer.start(53, "*", WiFi.softAPIP());
  ensureServerStarted();

  mode = MODE_PORTAL;
  mdnsStarted = false;
  drawPortalScreen();
  Serial.print("Wi-Fi wizard AP: ");
  Serial.println(AP_NAME);
  Serial.print("Open http://");
  Serial.println(WiFi.softAPIP());
  if (err) Serial.println(err);
}

void beginConnect() {
  showStatus("Connecting", wifiSsid.c_str(), "please wait");

  dnsServer.stop();
  WiFi.mode(WIFI_STA);
  delay(100);
  WiFi.setHostname("VibeMonitor");
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  WiFi.setAutoReconnect(true);
  WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());
  ensureServerStarted();

  connectStarted = millis();
  mode = MODE_CONNECTING;
  drawConnectingScreen();
  Serial.print("Connecting to ");
  Serial.println(wifiSsid);
}

void finishConnect() {
  mode = MODE_RUN;
  Serial.print("Connected, IP ");
  Serial.println(WiFi.localIP());
  if (!mdnsStarted) {
    mdnsStarted = MDNS.begin(MDNS_NAME);
    if (mdnsStarted) {
      MDNS.addService("http", "tcp", 80);
      Serial.println("http://vibemonitor.local");
    }
  }
}
