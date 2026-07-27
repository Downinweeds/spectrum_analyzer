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
 */

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <esp_timer.h>

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
volatile int      g_freqHz      = 10;
volatile int      g_durationMin = 5;
volatile bool     g_running     = false;
volatile bool     g_pulseHigh   = false;
volatile uint32_t g_periodUs    = 100000;
volatile uint64_t g_endMs       = 0;

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
void handleStart();
void handleStop();
void handleWifiInfo();
void handleScan();
void handleConnect();
void handleForgetWifi();
void handleNotFound();
String htmlEscape(const String& in);

// ============================================================
// Pulse timing
// ============================================================

void IRAM_ATTR onPulseTimer(void* /*arg*/) {
  if (!g_running) {
    digitalWrite(PULSE_PIN, LOW);
    g_pulseHigh = false;
    return;
  }

  if ((uint64_t)millis() >= g_endMs) {
    digitalWrite(PULSE_PIN, LOW);
    g_pulseHigh = false;
    g_running = false;
    return;
  }

  if (g_pulseHigh) {
    digitalWrite(PULSE_PIN, LOW);
    g_pulseHigh = false;
    uint32_t lowUs = (g_periodUs > PULSE_WIDTH_US)
                       ? (g_periodUs - PULSE_WIDTH_US)
                       : 1;
    esp_timer_start_once(pulseTimer, lowUs);
  } else {
    digitalWrite(PULSE_PIN, HIGH);
    g_pulseHigh = true;
    esp_timer_start_once(pulseTimer, PULSE_WIDTH_US);
  }
}

void scheduleNextEdge() {
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

String htmlEscape(const String& in) {
  String out;
  out.reserve(in.length() + 8);
  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    switch (c) {
      case '&':  out += F("&amp;");  break;
      case '<':  out += F("&lt;");   break;
      case '>':  out += F("&gt;");   break;
      case '"':  out += F("&quot;"); break;
      case '\'': out += F("&#39;");  break;
      default:   out += c;           break;
    }
  }
  return out;
}

// ============================================================
// Shared CSS (PROGMEM)
// ============================================================

static const char COMMON_CSS[] PROGMEM = R"CSS(
:root{
  --bg0:#0b1220;--bg1:#142033;--ink:#e8eef7;--muted:#93a4bd;
  --accent:#2ec4b6;--accent-dim:#1a8f85;--danger:#e35d6a;--danger-dim:#a33d48;
  --ok:#7ddea8;--card:rgba(255,255,255,0.04);--line:rgba(255,255,255,0.08);
}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
html,body{margin:0;min-height:100%;font-family:"Segoe UI",system-ui,-apple-system,sans-serif;color:var(--ink);
background:radial-gradient(1200px 600px at 10% -10%,#1b3a4b 0%,transparent 55%),
radial-gradient(900px 500px at 100% 0%,#24305a 0%,transparent 50%),
linear-gradient(165deg,var(--bg0),var(--bg1))}
.wrap{max-width:440px;margin:0 auto;padding:28px 20px 40px}
h1{margin:0 0 6px;font-size:1.5rem;font-weight:650;letter-spacing:.02em}
.sub{margin:0 0 22px;color:var(--muted);font-size:.95rem;line-height:1.4}
.panel{background:var(--card);border:1px solid var(--line);border-radius:18px;padding:18px 16px 14px;margin-bottom:14px}
.steps{display:flex;gap:8px;margin:0 0 20px}
.step{flex:1;height:6px;border-radius:999px;background:rgba(255,255,255,.12)}
.step.on{background:var(--accent)}
.row{display:flex;justify-content:space-between;align-items:baseline;margin-bottom:10px}
.label{font-size:.95rem;color:var(--muted)}
.value{font-variant-numeric:tabular-nums;font-size:1.35rem;font-weight:650;color:var(--accent)}
.hint{margin-top:6px;font-size:.8rem;color:var(--muted);line-height:1.35}
.ol{margin:0;padding-left:1.2rem;color:var(--ink);line-height:1.55}
.ol li{margin:0 0 8px}
.ol b{color:var(--accent)}
.btnrow{display:grid;gap:12px;margin:18px 0 10px}
.btnrow.two{grid-template-columns:1fr 1fr}
button,.btn{
  appearance:none;border:0;border-radius:14px;padding:15px 12px;font-size:1.02rem;font-weight:650;
  cursor:pointer;display:block;text-align:center;text-decoration:none;color:#062a27;
  background:linear-gradient(180deg,#3ad7c8,var(--accent-dim))
}
button.secondary,.btn.secondary{background:rgba(255,255,255,.08);color:var(--ink);border:1px solid var(--line)}
button.danger{background:linear-gradient(180deg,#f07a85,var(--danger-dim));color:#2a0a0e}
button:active,.btn:active{transform:scale(.98);opacity:.92}
button:disabled{opacity:.45;cursor:default;transform:none}
input[type=range]{-webkit-appearance:none;appearance:none;width:100%;height:36px;background:transparent;margin:0}
input[type=range]::-webkit-slider-runnable-track{height:10px;border-radius:999px;background:linear-gradient(90deg,var(--accent-dim),var(--accent))}
input[type=range]::-moz-range-track{height:10px;border-radius:999px;background:linear-gradient(90deg,var(--accent-dim),var(--accent))}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:28px;height:28px;margin-top:-9px;border-radius:50%;background:#fff;border:3px solid var(--accent)}
input[type=range]::-moz-range-thumb{width:28px;height:28px;border-radius:50%;background:#fff;border:3px solid var(--accent)}
input[type=password],input[type=text],select{
  width:100%;padding:14px 12px;border-radius:12px;border:1px solid var(--line);
  background:rgba(0,0,0,.25);color:var(--ink);font-size:1rem;margin:8px 0 4px
}
select option{background:#142033;color:var(--ink)}
.status{display:grid;gap:8px;padding:14px 16px;border-radius:14px;background:rgba(0,0,0,.22);border:1px solid rgba(255,255,255,.07);font-size:.95rem}
.status .k{color:var(--muted)}.status .v{font-variant-numeric:tabular-nums;font-weight:600}
.pill{display:inline-block;padding:3px 10px;border-radius:999px;font-size:.85rem;font-weight:700;letter-spacing:.04em}
.pill.on{background:rgba(125,222,168,.18);color:var(--ok)}
.pill.off{background:rgba(147,164,189,.16);color:var(--muted)}
.pill.warn{background:rgba(227,93,106,.18);color:#f0a0a8}
.msg{padding:12px 14px;border-radius:12px;background:rgba(46,196,182,.12);border:1px solid rgba(46,196,182,.28);color:var(--ink);font-size:.92rem;line-height:1.4;margin-bottom:14px}
.msg.err{background:rgba(227,93,106,.12);border-color:rgba(227,93,106,.3)}
.foot{margin-top:18px;text-align:center;color:var(--muted);font-size:.8rem;line-height:1.4}
.hidden{display:none!important}
.netlist{max-height:220px;overflow:auto;margin:8px 0 4px;border:1px solid var(--line);border-radius:12px}
.netbtn{width:100%;text-align:left;background:transparent;color:var(--ink);border:0;border-bottom:1px solid var(--line);
  border-radius:0;padding:12px 12px;font-weight:500;font-size:.95rem}
.netbtn:last-child{border-bottom:0}
.netbtn.sel{background:rgba(46,196,182,.15);color:var(--accent)}
.rssi{float:right;color:var(--muted);font-size:.8rem}
)CSS";

// ============================================================
// Wizard page
// ============================================================

static const char WIZARD_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<meta name="apple-mobile-web-app-capable" content="yes">
<title>WiFi Setup</title>
<style>)HTML";

// CSS is sent separately; we build wizard in handler for flexibility with dynamic bits.
// Full static wizard body:
static const char WIZARD_BODY[] PROGMEM = R"HTML(
</style>
</head>
<body>
<div class="wrap">
  <h1>WiFi Setup Wizard</h1>
  <p class="sub">Connect your phone to the XIAO pulse generator.</p>
  <div class="steps" id="stepBar">
    <div class="step on" id="s1"></div>
    <div class="step" id="s2"></div>
    <div class="step" id="s3"></div>
  </div>

  <div id="panel1">
    <div class="panel">
      <p class="hint" style="margin-top:0">You are talking to the XIAO over its setup hotspot.</p>
      <ol class="ol">
        <li>On your phone, open Wi‑Fi settings.</li>
        <li>Join network <b>XIAO-PulseGen</b>.</li>
        <li>Password: <b>pulse1234</b>.</li>
        <li>If a “Sign in” / captive portal banner appears, open it — that is this wizard.</li>
        <li>Otherwise open <b>http://192.168.4.1</b> in your browser.</li>
      </ol>
    </div>
    <div class="status" id="linkStatus">
      <div><span class="k">Link</span> · <span id="linkPill" class="pill off">CHECKING…</span></div>
      <div><span class="k">Device hotspot</span> · <span class="v">XIAO-PulseGen</span></div>
      <div><span class="k">Setup address</span> · <span class="v" id="apIp">192.168.4.1</span></div>
    </div>
    <div class="btnrow">
      <button type="button" id="btnNext1">I'm connected — continue</button>
    </div>
  </div>

  <div id="panel2" class="hidden">
    <div class="panel">
      <p class="hint" style="margin-top:0">How should the phone reach the XIAO?</p>
      <p style="margin:10px 0 0;line-height:1.45">
        <b>Device hotspot</b> — keep using <b>XIAO-PulseGen</b> (simplest, no home Wi‑Fi needed).<br><br>
        <b>Home Wi‑Fi</b> — put the XIAO on your router so your phone can keep internet and control the device on the same network.
      </p>
    </div>
    <div class="btnrow">
      <button type="button" id="btnUseAp">Use device hotspot</button>
      <button type="button" class="secondary" id="btnUseHome">Join home Wi‑Fi…</button>
    </div>
  </div>

  <div id="panel3" class="hidden">
    <div id="msg" class="msg hidden"></div>
    <div class="panel">
      <div class="row">
        <span class="label">Nearby networks</span>
        <button type="button" class="secondary" id="btnScan" style="padding:8px 12px;font-size:.9rem;width:auto">Refresh</button>
      </div>
      <div class="netlist" id="netList"><div class="hint" style="padding:12px">Scanning…</div></div>
      <label class="label" for="ssid">Network name (SSID)</label>
      <input id="ssid" type="text" autocomplete="off" autocapitalize="none" spellcheck="false" placeholder="Select or type SSID">
      <label class="label" for="pass">Password</label>
      <input id="pass" type="password" autocomplete="off" placeholder="Wi‑Fi password">
      <div class="hint">Saved on the XIAO for next boot. SoftAP setup stays available if join fails.</div>
    </div>
    <div class="btnrow two">
      <button type="button" class="secondary" id="btnBack2">Back</button>
      <button type="button" id="btnJoin">Join network</button>
    </div>
  </div>

  <div id="panel4" class="hidden">
    <div class="msg" id="doneMsg">Connected.</div>
    <div class="panel">
      <ol class="ol" id="doneSteps"></ol>
    </div>
    <div class="btnrow">
      <a class="btn" id="btnOpenControl" href="/control">Open pulse controls</a>
      <button type="button" class="secondary" id="btnWizardAgain">Run wizard again</button>
    </div>
  </div>

  <p class="foot">XIAO ESP32S3 · Pulse Generator setup</p>
</div>
<script>
const panels = [null,
  document.getElementById('panel1'),
  document.getElementById('panel2'),
  document.getElementById('panel3'),
  document.getElementById('panel4')];
const stepEls = [null,
  document.getElementById('s1'),
  document.getElementById('s2'),
  document.getElementById('s3')];

function showPanel(n) {
  for (let i = 1; i <= 4; i++) panels[i].classList.toggle('hidden', i !== n);
  const map = {1:1, 2:2, 3:2, 4:3};
  const on = map[n] || 1;
  for (let i = 1; i <= 3; i++) stepEls[i].classList.toggle('on', i <= on);
  try { localStorage.setItem('pg_wiz_step', String(n)); } catch(e) {}
}

function showMsg(text, err) {
  const el = document.getElementById('msg');
  el.textContent = text;
  el.classList.remove('hidden');
  el.classList.toggle('err', !!err);
}

async function refreshInfo() {
  try {
    const r = await fetch('/wifi-info');
    const j = await r.json();
    document.getElementById('apIp').textContent = j.apIp || '192.168.4.1';
    const pill = document.getElementById('linkPill');
    pill.textContent = 'ONLINE';
    pill.className = 'pill on';
    if (j.staConnected) {
      // Already on home Wi‑Fi — offer jump ahead
      document.getElementById('doneMsg').textContent =
        'XIAO is on Wi‑Fi “' + j.staSsid + '” at ' + j.staIp + '.';
      const ol = document.getElementById('doneSteps');
      ol.innerHTML =
        '<li>Join the same home Wi‑Fi on your phone if you are still on the setup hotspot.</li>' +
        '<li>Open <b>http://' + j.staIp + '/control</b> (or tap below while still on the hotspot).</li>';
      document.getElementById('btnOpenControl').href = '/control';
    }
    return j;
  } catch (e) {
    const pill = document.getElementById('linkPill');
    pill.textContent = 'NO LINK';
    pill.className = 'pill warn';
    return null;
  }
}

async function scanNets() {
  const box = document.getElementById('netList');
  box.innerHTML = '<div class="hint" style="padding:12px">Scanning…</div>';
  try {
    const r = await fetch('/scan');
    const j = await r.json();
    if (!j.networks || !j.networks.length) {
      box.innerHTML = '<div class="hint" style="padding:12px">No networks found. Tap Refresh.</div>';
      return;
    }
    box.innerHTML = '';
    j.networks.forEach(n => {
      const b = document.createElement('button');
      b.type = 'button';
      b.className = 'netbtn';
      b.innerHTML = (n.ssid || '(hidden)') +
        '<span class="rssi">' + (n.secure ? '🔒 ' : '') + n.rssi + ' dBm</span>';
      b.addEventListener('click', () => {
        document.querySelectorAll('.netbtn').forEach(x => x.classList.remove('sel'));
        b.classList.add('sel');
        document.getElementById('ssid').value = n.ssid || '';
      });
      box.appendChild(b);
    });
  } catch (e) {
    box.innerHTML = '<div class="hint" style="padding:12px">Scan failed. Tap Refresh.</div>';
  }
}

document.getElementById('btnNext1').onclick = () => showPanel(2);
document.getElementById('btnUseAp').onclick = () => {
  try { localStorage.setItem('pg_wiz_done', 'ap'); } catch(e) {}
  document.getElementById('doneMsg').textContent =
    'Using the XIAO hotspot. Stay on Wi‑Fi XIAO-PulseGen.';
  document.getElementById('doneSteps').innerHTML =
    '<li>Keep your phone on <b>XIAO-PulseGen</b>.</li>' +
    '<li>Open the pulse control page (button below).</li>' +
    '<li>Note: your phone may not have internet while on this hotspot.</li>';
  document.getElementById('btnOpenControl').href = '/control';
  showPanel(4);
};
document.getElementById('btnUseHome').onclick = async () => {
  showPanel(3);
  await scanNets();
};
document.getElementById('btnBack2').onclick = () => showPanel(2);
document.getElementById('btnScan').onclick = () => scanNets();
document.getElementById('btnWizardAgain').onclick = () => {
  try { localStorage.removeItem('pg_wiz_done'); } catch(e) {}
  showPanel(1);
};
document.getElementById('btnJoin').onclick = async () => {
  const ssid = document.getElementById('ssid').value.trim();
  const pass = document.getElementById('pass').value;
  if (!ssid) { showMsg('Enter or select a network name.', true); return; }
  const btn = document.getElementById('btnJoin');
  btn.disabled = true;
  showMsg('Joining “' + ssid + '”… this can take up to 20 seconds.', false);
  try {
    const body = 'ssid=' + encodeURIComponent(ssid) + '&pass=' + encodeURIComponent(pass);
    const r = await fetch('/connect', {
      method: 'POST',
      headers: {'Content-Type': 'application/x-www-form-urlencoded'},
      body
    });
    const j = await r.json();
    if (j.ok) {
      try { localStorage.setItem('pg_wiz_done', 'sta'); } catch(e) {}
      document.getElementById('doneMsg').textContent =
        'XIAO joined “' + j.ssid + '”. Address: ' + j.ip;
      document.getElementById('doneSteps').innerHTML =
        '<li>Switch your phone Wi‑Fi from <b>XIAO-PulseGen</b> to <b>' + j.ssid + '</b>.</li>' +
        '<li>Open <b>http://' + j.ip + '/control</b> in your browser.</li>' +
        '<li>Setup hotspot remains available if you need the wizard again.</li>';
      document.getElementById('btnOpenControl').href = 'http://' + j.ip + '/control';
      showPanel(4);
    } else {
      showMsg(j.message || 'Join failed. Check password and try again.', true);
    }
  } catch (e) {
    showMsg('Request failed. Stay on XIAO-PulseGen and retry.', true);
  } finally {
    btn.disabled = false;
  }
};

(async () => {
  const info = await refreshInfo();
  // If already finished wizard in this browser and link is up, go control
  try {
    if (localStorage.getItem('pg_wiz_done') && info) {
      location.replace('/control');
      return;
    }
  } catch(e) {}
  showPanel(1);
  setInterval(refreshInfo, 3000);
})();
</script>
</body>
</html>
)HTML";

// ============================================================
// Control page
// ============================================================

static const char CONTROL_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<meta name="apple-mobile-web-app-capable" content="yes">
<title>Pulse Generator</title>
<style>)HTML";

static const char CONTROL_BODY[] PROGMEM = R"HTML(
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
    <div class="hint">1 – 500 Hz · pulse width fixed at 1 ms</div>
  </div>

  <div class="panel">
    <div class="row">
      <span class="label">Duration</span>
      <span class="value"><span id="durVal">5</span> min</span>
    </div>
    <input id="dur" type="range" min="1" max="60" step="1" value="5">
    <div class="hint">1 – 60 minutes</div>
  </div>

  <div class="btnrow two">
    <button id="btnStart" type="button">Start</button>
    <button id="btnStop" class="danger" type="button">Stop</button>
  </div>

  <div class="status">
    <div><span class="k">State</span> · <span id="statePill" class="pill off">IDLE</span></div>
    <div><span class="k">Output</span> · <span class="v" id="outFreq">—</span></div>
    <div><span class="k">Remaining</span> · <span class="v" id="remain">—</span></div>
    <div><span class="k">Wi‑Fi</span> · <span class="v" id="wifiLine">—</span></div>
  </div>

  <div class="btnrow" style="margin-top:14px">
    <a class="btn secondary" href="/wizard">Wi‑Fi setup wizard</a>
  </div>
  <p class="foot" id="foot">Pulse control</p>
</div>
<script>
const freq = document.getElementById('freq');
const dur = document.getElementById('dur');
const freqVal = document.getElementById('freqVal');
const durVal = document.getElementById('durVal');
freq.oninput = () => freqVal.textContent = freq.value;
dur.oninput = () => durVal.textContent = dur.value;

function fmtRemain(sec) {
  if (sec == null || sec < 0) return '—';
  const m = Math.floor(sec / 60), s = Math.floor(sec % 60);
  return m + 'm ' + String(s).padStart(2,'0') + 's';
}

document.getElementById('btnStart').onclick = async () => {
  const b = document.getElementById('btnStart');
  b.disabled = true;
  try {
    await fetch('/start?freq=' + encodeURIComponent(freq.value) +
                '&duration=' + encodeURIComponent(dur.value));
    await refresh();
  } finally { b.disabled = false; }
};
document.getElementById('btnStop').onclick = async () => {
  const b = document.getElementById('btnStop');
  b.disabled = true;
  try { await fetch('/stop'); await refresh(); }
  finally { b.disabled = false; }
};

async function refresh() {
  try {
    const [rs, rw] = await Promise.all([fetch('/status'), fetch('/wifi-info')]);
    const j = await rs.json();
    const w = await rw.json();
    freq.value = j.freq; dur.value = j.duration;
    freqVal.textContent = j.freq; durVal.textContent = j.duration;
    const pill = document.getElementById('statePill');
    if (j.running) {
      pill.textContent = 'RUNNING'; pill.className = 'pill on';
      document.getElementById('outFreq').textContent = j.freq + ' Hz · 1 ms';
      document.getElementById('remain').textContent = fmtRemain(j.remainingSec);
      freq.disabled = true; dur.disabled = true;
    } else {
      pill.textContent = 'IDLE'; pill.className = 'pill off';
      document.getElementById('outFreq').textContent = 'off';
      document.getElementById('remain').textContent = '—';
      freq.disabled = false; dur.disabled = false;
    }
    let line = 'hotspot ' + (w.apIp || '192.168.4.1');
    if (w.staConnected) line = w.staSsid + ' · ' + w.staIp;
    document.getElementById('wifiLine').textContent = line;
    document.getElementById('foot').textContent = w.staConnected
      ? ('On home Wi‑Fi · http://' + w.staIp + '/control')
      : ('On setup hotspot · http://' + w.apIp + '/control');
  } catch (e) {
    document.getElementById('statePill').textContent = 'OFFLINE';
    document.getElementById('statePill').className = 'pill warn';
  }
}
refresh();
setInterval(refresh, 1000);
</script>
</body>
</html>
)HTML";

// ============================================================
// HTTP helpers / handlers
// ============================================================

void sendHtmlParts(const char* headProgmem, const char* bodyProgmem) {
  // Stream CSS + page to avoid one huge RAM buffer
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

  // Common OS captive-portal check paths
  if (uri.indexOf(F("generate_204")) >= 0) return true;          // Android
  if (uri.indexOf(F("gen_204")) >= 0) return true;
  if (uri.indexOf(F("hotspot-detect")) >= 0) return true;        // iOS / macOS
  if (uri.indexOf(F("library/test")) >= 0) return true;          // iOS
  if (uri.indexOf(F("ncsi.txt")) >= 0) return true;              // Windows
  if (uri.indexOf(F("connecttest")) >= 0) return true;           // Windows
  if (uri.indexOf(F("captive")) >= 0) return true;
  if (uri == F("/fwlink")) return true;

  // Any Host that is not our AP/STA IP → captive redirect candidate
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
  // Captive portal and first visit land on the wizard
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
  // Block while scanning; keep SoftAP up
  int n = WiFi.scanNetworks(/*async=*/false, /*hidden=*/true);
  String json = "{\"networks\":[";
  // Sort-ish: include up to 20 strongest by walking once
  int used = 0;
  for (int i = 0; i < n && used < 20; i++) {
    String ssid = WiFi.SSID(i);
    if (ssid.length() == 0) {
      continue;
    }
    if (used) json += ',';
    json += "{\"ssid\":\"";
    // Minimal JSON escape for quotes/backslashes
    for (size_t k = 0; k < ssid.length(); k++) {
      char c = ssid[k];
      if (c == '"' || c == '\\') json += '\\';
      json += c;
    }
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

  // Keep SoftAP up so the phone can finish the wizard / recover
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
  json += '}';
  server.send(200, "application/json", json);
}

void handleStart() {
  int freq = g_freqHz;
  int dur  = g_durationMin;
  if (server.hasArg("freq")) freq = server.arg("freq").toInt();
  if (server.hasArg("duration")) dur = server.arg("duration").toInt();
  freq = constrain(freq, FREQ_MIN_HZ, FREQ_MAX_HZ);
  dur  = constrain(dur, DUR_MIN_MIN, DUR_MAX_MIN);
  g_freqHz = freq;
  g_durationMin = dur;
  startPulseTrain();
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleStop() {
  stopPulseTrain();
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleNotFound() {
  if (isCaptivePortalProbe()) {
    // Redirect OS captive checks into the wizard
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
  server.on("/start", HTTP_GET, handleStart);
  server.on("/stop", HTTP_GET, handleStop);
  server.on("/wifi-info", HTTP_GET, handleWifiInfo);
  server.on("/scan", HTTP_GET, handleScan);
  server.on("/connect", HTTP_POST, handleConnect);
  // Also accept GET connect for simple clients
  server.on("/connect", HTTP_GET, handleConnect);
  server.on("/forget-wifi", HTTP_POST, handleForgetWifi);
  server.on("/forget-wifi", HTTP_GET, handleForgetWifi);

  // Explicit captive-portal endpoints
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

  // Optional: reconnect to saved home Wi‑Fi
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
      Serial.println(F("STA failed — SoftAP wizard still available"));
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

  if (g_running && (uint64_t)millis() >= g_endMs) {
    stopPulseTrain();
  }
}
