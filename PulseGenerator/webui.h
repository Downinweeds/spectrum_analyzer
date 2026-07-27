#pragma once
// Auto-kept web UI strings for PulseGenerator (kept out of the .ino so
// Arduino's prototype generator does not parse JavaScript as C++).

#include <pgmspace.h>

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

static const char WIZARD_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<meta name="apple-mobile-web-app-capable" content="yes">
<title>WiFi Setup</title>
<style>)HTML";

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
