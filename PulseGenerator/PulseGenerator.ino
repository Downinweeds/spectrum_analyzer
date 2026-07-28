/*
 * XIAO ESP32S3 Pulse Generator
 * ----------------------------
 * Outputs 1 ms pulses on pin D10 at a selectable frequency (1–500 Hz).
 * Frequency and run duration (1–60 minutes) are set from a smartphone
 * over WiFi via an embedded web page with graphic sliders.
 *
 * Includes a WiFi connection wizard with captive portal:
 *   - Join SoftAP "XIAO-PulseGen" (password: pulse1234)
 *   - Phone captive-portal / sign-in page opens the wizard
 *   - Use the device hotspot, or join your home WiFi
 *
 * Board:  Seeed XIAO ESP32S3
 * Pin:    D10 (GPIO9)
 *
 * Files:
 *   PulseGenerator.ino  - device logic
 *   webui.h             - HTML/CSS/JS for phone UI (separate so the
 *                         Arduino IDE preprocessor does not mis-parse JS)
 */

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <esp_timer.h>
#include "webui.h"

// -------- Hardware --------
static const int PULSE_PIN = D10;             // XIAO ESP32S3 D10 = GPIO9
static const uint32_t PULSE_WIDTH_US = 1000;  // fixed 1 ms high time

// -------- SoftAP (setup / wizard network) --------
static const char* AP_SSID     = "XIAO-PulseGen";
static const char* AP_PASSWORD = "pulse1234";  // min 8 chars for WPA2
static const byte  DNS_PORT    = 53;

// -------- Limits --------
static const int FREQ_MIN_HZ = 1;
static const int FREQ_MAX_HZ = 500;
static const int DUR_MIN_MIN = 1;
static const int DUR_MAX_MIN = 60;

// -------- Runtime pulse state --------
struct PulseState {
  int      frequencyHz;
  int      durationMin;
  bool     running;
  bool     pulseHigh;
  uint32_t periodUs;
  uint64_t endMs;
};

PulseState pulse = {
  10,      // frequencyHz
  5,       // durationMin
  false,   // running
  false,   // pulseHigh
  100000,  // periodUs
  0        // endMs
};

// -------- WiFi / wizard state --------
Preferences prefs;
DNSServer dnsServer;
WebServer server(80);
esp_timer_handle_t pulseTimer = nullptr;

bool g_apActive = false;
bool g_staConnected = false;
String g_staSsid;
String g_connectMessage;
bool g_connectBusy = false;

static int clampInt(int value, int low, int high) {
  if (value < low) return low;
  if (value > high) return high;
  return value;
}

// Forward declarations
void IRAM_ATTR onPulseTimer(void* arg);
void scheduleNextEdge();
void startPulseTrain();
void stopPulseTrain();
void loadWifiPrefs();
void saveWifiPrefs(const String& ssid, const String& pass);
void clearWifiPrefs();
bool tryConnectSta(const String& ssid, const String& pass, uint32_t timeoutMs);
void startSoftAp();
void setupHttpRoutes();
bool isCaptivePortalProbe();
void handleRoot();
void handleWizard();
void handleControl();
void handleStatus();
void handleSet();
void handleStart();
void handleStop();
void handleWifiInfo();
void handleScan();
void handleConnect();
void handleForgetWifi();
void handleNotFound();
String jsonEscape(const String& in);
void sendHtmlParts(const char* headProgmem, const char* bodyProgmem);

// ============================================================
// Pulse timing
// ============================================================

void IRAM_ATTR onPulseTimer(void* /*arg*/) {
  if (!pulse.running) {
    digitalWrite(PULSE_PIN, LOW);
    pulse.pulseHigh = false;
    return;
  }

  if ((uint64_t)millis() >= pulse.endMs) {
    digitalWrite(PULSE_PIN, LOW);
    pulse.pulseHigh = false;
    pulse.running = false;
    return;
  }

  if (pulse.pulseHigh) {
    digitalWrite(PULSE_PIN, LOW);
    pulse.pulseHigh = false;
    uint32_t lowUs = (pulse.periodUs > PULSE_WIDTH_US)
                       ? (pulse.periodUs - PULSE_WIDTH_US)
                       : 1;
    esp_timer_start_once(pulseTimer, lowUs);
  } else {
    digitalWrite(PULSE_PIN, HIGH);
    pulse.pulseHigh = true;
    esp_timer_start_once(pulseTimer, PULSE_WIDTH_US);
  }
}

void scheduleNextEdge() {
  digitalWrite(PULSE_PIN, HIGH);
  pulse.pulseHigh = true;
  esp_timer_start_once(pulseTimer, PULSE_WIDTH_US);
}

void startPulseTrain() {
  if (pulse.running) {
    stopPulseTrain();
  }

  int freq = clampInt(pulse.frequencyHz, FREQ_MIN_HZ, FREQ_MAX_HZ);
  int dur  = clampInt(pulse.durationMin, DUR_MIN_MIN, DUR_MAX_MIN);

  pulse.frequencyHz = freq;
  pulse.durationMin = dur;
  pulse.periodUs    = 1000000UL / (uint32_t)freq;
  pulse.endMs       = (uint64_t)millis() + (uint64_t)dur * 60000ULL;
  pulse.running     = true;

  scheduleNextEdge();
}

void stopPulseTrain() {
  pulse.running = false;
  esp_timer_stop(pulseTimer);
  digitalWrite(PULSE_PIN, LOW);
  pulse.pulseHigh = false;
}

// ============================================================
// WiFi helpers
// ============================================================

void loadWifiPrefs() {
  prefs.begin("pulsegen", true);
  g_staSsid = prefs.getString("ssid", "");
  prefs.end();
}

void saveWifiPrefs(const String& ssid, const String& pass) {
  prefs.begin("pulsegen", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.end();
  g_staSsid = ssid;
}

void clearWifiPrefs() {
  prefs.begin("pulsegen", false);
  prefs.clear();
  prefs.end();
  g_staSsid = "";
}

bool tryConnectSta(const String& ssid, const String& pass, uint32_t timeoutMs) {
  if (ssid.length() == 0) {
    return false;
  }

  WiFi.mode(WIFI_AP_STA);
  WiFi.disconnect(false, false);
  delay(100);
  WiFi.begin(ssid.c_str(), pass.c_str());

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
    delay(200);
  }

  g_staConnected = (WiFi.status() == WL_CONNECTED);
  return g_staConnected;
}

void startSoftAp() {
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  delay(100);
  g_apActive = true;

  IPAddress apIp = WiFi.softAPIP();
  dnsServer.start(DNS_PORT, "*", apIp);

  Serial.printf("SoftAP SSID: %s\n", AP_SSID);
  Serial.printf("SoftAP pass: %s\n", AP_PASSWORD);
  Serial.printf("Wizard URL:  http://%s/\n", apIp.toString().c_str());
}

// ============================================================
// HTTP helpers / handlers
// ============================================================

void sendHtmlParts(const char* headProgmem, const char* bodyProgmem) {
  const size_t len = strlen_P(headProgmem) + strlen_P(COMMON_CSS) + strlen_P(bodyProgmem);
  server.setContentLength(len);
  server.send(200, "text/html", "");
  server.sendContent_P(headProgmem);
  server.sendContent_P(COMMON_CSS);
  server.sendContent_P(bodyProgmem);
}

String jsonEscape(const String& in) {
  String out;
  out.reserve(in.length() + 8);
  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    if (c == '"' || c == '\\') out += '\\';
    else if (c == '\n') { out += "\\n"; continue; }
    else if (c == '\r') { out += "\\r"; continue; }
    out += c;
  }
  return out;
}

bool isCaptivePortalProbe() {
  String host = server.hostHeader();
  String uri  = server.uri();

  if (uri.indexOf(F("generate_204")) >= 0) return true;
  if (uri.indexOf(F("gen_204")) >= 0) return true;
  if (uri.indexOf(F("hotspot-detect")) >= 0) return true;
  if (uri.indexOf(F("library/test")) >= 0) return true;
  if (uri.indexOf(F("ncsi.txt")) >= 0) return true;
  if (uri.indexOf(F("connecttest")) >= 0) return true;
  if (uri.indexOf(F("captive")) >= 0) return true;
  if (uri == F("/fwlink")) return true;

  if (host.length()) {
    String ap = WiFi.softAPIP().toString();
    String sta = WiFi.isConnected() ? WiFi.localIP().toString() : String();
    if (host != ap && host != sta && host != F("localhost") &&
        host.indexOf(F("192.168.4.")) < 0) {
      return true;
    }
  }
  return false;
}

void handleWizard() {
  sendHtmlParts(WIZARD_HTML, WIZARD_BODY);
}

void handleControl() {
  sendHtmlParts(CONTROL_HTML, CONTROL_BODY);
}

void handleRoot() {
  handleWizard();
}

void handleWifiInfo() {
  g_staConnected = (WiFi.status() == WL_CONNECTED);
  String staName = g_staConnected ? WiFi.SSID() : g_staSsid;

  String json = "{";
  json += "\"apActive\":";
  json += g_apActive ? "true" : "false";
  json += ",\"apSsid\":\"";
  json += AP_SSID;
  json += "\",\"apIp\":\"";
  json += WiFi.softAPIP().toString();
  json += "\",\"staConnected\":";
  json += g_staConnected ? "true" : "false";
  json += ",\"staSsid\":\"";
  json += jsonEscape(staName);
  json += "\",\"staIp\":\"";
  json += g_staConnected ? WiFi.localIP().toString() : "";
  json += "\"}";
  server.send(200, "application/json", json);
}

void handleScan() {
  int n = WiFi.scanNetworks(false, true);
  String json = "{\"networks\":[";
  int used = 0;
  for (int i = 0; i < n && used < 20; i++) {
    String ssid = WiFi.SSID(i);
    if (ssid.length() == 0) {
      continue;
    }
    if (used) json += ',';
    json += "{\"ssid\":\"";
    json += jsonEscape(ssid);
    json += "\",\"rssi\":";
    json += String(WiFi.RSSI(i));
    json += ",\"secure\":";
    json += (WiFi.encryptionType(i) != WIFI_AUTH_OPEN) ? "true" : "false";
    json += '}';
    used++;
  }
  json += "]}";
  WiFi.scanDelete();
  server.send(200, "application/json", json);
}

void handleConnect() {
  if (g_connectBusy) {
    server.send(200, "application/json",
                "{\"ok\":false,\"message\":\"Connection already in progress\"}");
    return;
  }

  String ssid = server.hasArg("ssid") ? server.arg("ssid") : "";
  String pass = server.hasArg("pass") ? server.arg("pass") : "";
  ssid.trim();

  if (ssid.length() == 0 || ssid.length() > 32) {
    server.send(200, "application/json",
                "{\"ok\":false,\"message\":\"Invalid SSID\"}");
    return;
  }

  g_connectBusy = true;
  bool ok = tryConnectSta(ssid, pass, 18000);

  WiFi.mode(WIFI_AP_STA);
  startSoftAp();

  if (ok) {
    saveWifiPrefs(ssid, pass);
    g_connectMessage = "Connected";
    String json = "{\"ok\":true,\"ssid\":\"";
    json += jsonEscape(ssid);
    json += "\",\"ip\":\"";
    json += WiFi.localIP().toString();
    json += "\"}";
    g_connectBusy = false;
    server.send(200, "application/json", json);
  } else {
    g_connectBusy = false;
    server.send(200, "application/json",
                "{\"ok\":false,\"message\":\"Could not join that network. Check the password and try again.\"}");
  }
}

void handleForgetWifi() {
  clearWifiPrefs();
  WiFi.disconnect(false, true);
  g_staConnected = false;
  WiFi.mode(WIFI_AP);
  startSoftAp();
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleStatus() {
  int remainingSec = 0;
  if (pulse.running) {
    uint64_t now = (uint64_t)millis();
    if (now < pulse.endMs) {
      remainingSec = (int)((pulse.endMs - now) / 1000ULL);
    }
  }

  String json = "{";
  json += "\"running\":";
  json += pulse.running ? "true" : "false";
  json += ",\"freq\":";
  json += String(pulse.frequencyHz);
  json += ",\"duration\":";
  json += String(pulse.durationMin);
  json += ",\"pulseUs\":";
  json += String((unsigned long)PULSE_WIDTH_US);
  json += ",\"remainingSec\":";
  json += String(remainingSec);
  json += '}';
  server.send(200, "application/json", json);
}

void handleSet() {
  // Update stored frequency / duration without starting the pulse train.
  // Used by the phone UI when a slider is released.
  if (server.hasArg("freq")) {
    pulse.frequencyHz = clampInt(server.arg("freq").toInt(), FREQ_MIN_HZ, FREQ_MAX_HZ);
  }
  if (server.hasArg("duration")) {
    pulse.durationMin = clampInt(server.arg("duration").toInt(), DUR_MIN_MIN, DUR_MAX_MIN);
  }
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleStart() {
  int freq = pulse.frequencyHz;
  int dur  = pulse.durationMin;
  if (server.hasArg("freq")) freq = server.arg("freq").toInt();
  if (server.hasArg("duration")) dur = server.arg("duration").toInt();
  pulse.frequencyHz = clampInt(freq, FREQ_MIN_HZ, FREQ_MAX_HZ);
  pulse.durationMin = clampInt(dur, DUR_MIN_MIN, DUR_MAX_MIN);
  startPulseTrain();
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleStop() {
  stopPulseTrain();
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleNotFound() {
  if (isCaptivePortalProbe()) {
    IPAddress apIp = WiFi.softAPIP();
    String loc = "http://" + apIp.toString() + "/wizard";
    server.sendHeader("Location", loc, true);
    server.send(302, "text/plain", "");
    return;
  }
  server.send(404, "text/plain", "Not found");
}

void setupHttpRoutes() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/wizard", HTTP_GET, handleWizard);
  server.on("/control", HTTP_GET, handleControl);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/set", HTTP_GET, handleSet);
  server.on("/start", HTTP_GET, handleStart);
  server.on("/stop", HTTP_GET, handleStop);
  server.on("/wifi-info", HTTP_GET, handleWifiInfo);
  server.on("/scan", HTTP_GET, handleScan);
  server.on("/connect", HTTP_POST, handleConnect);
  server.on("/connect", HTTP_GET, handleConnect);
  server.on("/forget-wifi", HTTP_POST, handleForgetWifi);
  server.on("/forget-wifi", HTTP_GET, handleForgetWifi);

  server.on("/generate_204", HTTP_GET, []() {
    server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/wizard", true);
    server.send(302, "text/plain", "");
  });
  server.on("/hotspot-detect.html", HTTP_GET, handleWizard);
  server.on("/library/test/success.html", HTTP_GET, handleWizard);
  server.on("/ncsi.txt", HTTP_GET, []() { server.send(200, "text/plain", "Microsoft NCSI"); });
  server.on("/connecttest.txt", HTTP_GET, []() { server.send(200, "text/plain", "Microsoft Connect Test"); });
  server.on("/fwlink", HTTP_GET, handleWizard);

  server.onNotFound(handleNotFound);
  server.begin();
}

// ============================================================
// Setup / loop
// ============================================================

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(PULSE_PIN, OUTPUT);
  digitalWrite(PULSE_PIN, LOW);

  const esp_timer_create_args_t timerArgs = {
    .callback = &onPulseTimer,
    .arg = nullptr,
    .dispatch_method = ESP_TIMER_TASK,
    .name = "pulse",
    .skip_unhandled_events = true
  };
  esp_timer_create(&timerArgs, &pulseTimer);

  Serial.println();
  Serial.println(F("=== XIAO ESP32S3 Pulse Generator ==="));

  loadWifiPrefs();
  WiFi.mode(WIFI_AP_STA);
  startSoftAp();

  if (g_staSsid.length()) {
    prefs.begin("pulsegen", true);
    String pass = prefs.getString("pass", "");
    prefs.end();
    Serial.printf("Trying saved Wi‑Fi: %s\n", g_staSsid.c_str());
    if (tryConnectSta(g_staSsid, pass, 12000)) {
      Serial.printf("STA OK  %s  http://%s/\n",
                    WiFi.SSID().c_str(),
                    WiFi.localIP().toString().c_str());
    } else {
      Serial.println(F("STA failed - SoftAP wizard still available"));
      WiFi.mode(WIFI_AP);
      startSoftAp();
    }
  }

  setupHttpRoutes();
  Serial.printf("Pulse pin: D10 | width: %lu us\n", (unsigned long)PULSE_WIDTH_US);
  Serial.println(F("Open the captive-portal / http://192.168.4.1 for the WiFi wizard"));
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();

  g_staConnected = (WiFi.status() == WL_CONNECTED);

  if (pulse.running && (uint64_t)millis() >= pulse.endMs) {
    stopPulseTrain();
  }
}
