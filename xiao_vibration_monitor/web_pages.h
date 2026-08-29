#pragma once

// HTML served by the XIAO. Keep these pages self-contained (inline CSS/JS).

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
  if (typeof d.sensitivity === "number") {
    document.getElementById("sens").value = d.sensitivity;
    document.getElementById("sensVal").textContent = d.sensitivity;
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

document.getElementById("sens").addEventListener("change", async (ev) => {
  const v = ev.target.value;
  document.getElementById("sensVal").textContent = v;
  await fetch("/api/sensitivity?value=" + v, { method: "POST" });
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
