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
#include "web_pages.h"

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

int sensitivity = 50;     // 10..100, higher = more sensitive
int amplitude = 0;        // 0..100 relative
int peakAmplitude = 0;
int lastRawP2P = 0;
bool detected = false;
uint32_t peakStamp = 0;

float noiseFloor = 40.0f;
float recentMax = 220.0f;

uint32_t lastSample = 0;
uint32_t lastOled = 0;
uint32_t detectHoldUntil = 0;

void drawCentered(int y, const char* text);
void showBootScreen();
void drawPortalScreen();
void drawConnectingScreen();
void drawRunScreen();
void sampleVibration();
void startPortal(const char* err = nullptr);
void beginConnect();
void finishConnect();
void setupServer();
void handleRoot();
void handleWizard();
void handleWifiSave();
void handleApiStatus();
void handleApiScan();
void handleApiSensitivity();
void handleCaptive();
bool bootButtonHeld();

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

  prefs.begin("vibe", false);
  wifiSsid = prefs.getString("ssid", "");
  wifiPass = prefs.getString("pass", "");
  sensitivity = constrain(prefs.getInt("sens", 50), 10, 100);

  setupServer();

  const bool forceWizard = bootButtonHeld();
  if (forceWizard) {
    Serial.println("USER button held — opening Wi-Fi wizard");
  }

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

  const float excess = max(0.0f, lastRawP2P - noiseFloor);
  recentMax = max(recentMax * 0.997f, max(180.0f, lastRawP2P));
  const float gain = sensitivity / 50.0f;
  const float scale = max(160.0f, recentMax * 0.80f);
  amplitude = constrain((int)lroundf(excess * gain * 100.0f / scale), 0, 100);

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
    peakAmplitude = max(0, (int)lroundf(peakAmplitude * 0.96f));
  }
}

void showBootScreen() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tf);
  drawCentered(18, "Vibe Monitor");
  u8g2.setFont(u8g2_font_5x8_tf);
  drawCentered(40, "XIAO ESP32-C3");
  drawCentered(52, "starting...");
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
  server.on("/api/status", handleApiStatus);
  server.on("/api/scan", handleApiScan);
  server.on("/api/sensitivity", HTTP_POST, handleApiSensitivity);

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

  server.begin();
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

void startPortal(const char* err) {
  dnsServer.stop();
  WiFi.disconnect(true, true);
  delay(100);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_NAME);
  delay(150);
  dnsServer.start(53, "*", WiFi.softAPIP());
  mode = MODE_PORTAL;
  mdnsStarted = false;
  Serial.print("Wi-Fi wizard AP: ");
  Serial.println(AP_NAME);
  Serial.print("Open http://");
  Serial.println(WiFi.softAPIP());
  if (err) Serial.println(err);
}

void beginConnect() {
  dnsServer.stop();
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("VibeMonitor");
  WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());
  connectStarted = millis();
  mode = MODE_CONNECTING;
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
