#!/usr/bin/env python3
"""Serve the sketch's HTML locally with mock vibration data for UI checks."""

from __future__ import annotations

import json
import math
import re
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "xiao_vibration_monitor.ino"

sensitivity = 50
avg_window = 1.0
ntfy_topic = ""
t0 = time.time()
hist: list[float] = []
pending_on = False
pending_since = t0
reported_on = False
reported_ready = False

# Preview stands in for stored cal: OFF mean 40, ON mean 200.
CAL_OFF_MEAN = 40.0
CAL_ON_MEAN = 200.0


def extract_html(symbol: str) -> str:
    text = HEADER.read_text()
    match = re.search(
        rf"{re.escape(symbol)}\[\] PROGMEM = R\"html\((.*?)\)html\";",
        text,
        re.S,
    )
    if not match:
        raise SystemExit(f"Could not find {symbol} in {HEADER}")
    return match.group(1)


DASHBOARD = extract_html("DASHBOARD_HTML")
WIZARD = extract_html("WIZARD_HTML")
CONNECTING = extract_html("CONNECTING_HTML")


def mock_status() -> dict:
    global pending_on, pending_since, reported_on, reported_ready
    elapsed = time.time() - t0
    chatter = 3 + 2 * abs(math.sin(elapsed * 7.0))
    # Alternate ~8 s ON / 8 s OFF so the 4 s ntfy-stable pill can catch up.
    machine_on = (elapsed % 16.0) < 8.0
    raw = int(CAL_OFF_MEAN + chatter * 3 + (CAL_ON_MEAN - CAL_OFF_MEAN) * (1.0 if machine_on else 0.0))
    span = CAL_ON_MEAN - CAL_OFF_MEAN
    amplitude = int(max(0, min(100, (raw - CAL_OFF_MEAN) * 100.0 / span)))
    hist.append(float(amplitude))
    if len(hist) > 400:
        del hist[: len(hist) - 400]
    n = max(1, int(round(avg_window / 0.25))) if avg_window > 0 else 1
    if avg_window <= 0:
        smoothed = amplitude
    else:
        window = hist[-n:]
        smoothed = int(round(sum(window) / len(window)))

    now = time.time()
    if machine_on != pending_on:
        pending_on = machine_on
        pending_since = now
    elif now - pending_since >= 4.0:
        if not reported_ready:
            reported_on = pending_on
            reported_ready = True
        else:
            reported_on = pending_on

    return {
        "amplitude": smoothed,
        "peak": max(smoothed, amplitude),
        "raw": raw,
        "detected": smoothed >= 12,
        "sensitivity": sensitivity,
        "avgWindow": round(avg_window, 1),
        "calibrated": True,
        "machineOn": machine_on,
        "reportedOn": reported_on,
        "reportedReady": reported_ready,
        "calOnMean": CAL_ON_MEAN,
        "calOnP90": CAL_ON_MEAN + 24,
        "calOffMean": CAL_OFF_MEAN,
        "calOffP90": CAL_OFF_MEAN + 8,
        "calOnThresh": CAL_OFF_MEAN + (CAL_ON_MEAN - CAL_OFF_MEAN) * 0.28,
        "calOffThresh": CAL_OFF_MEAN + (CAL_ON_MEAN - CAL_OFF_MEAN) * 0.12,
        "ntfyTopic": ntfy_topic,
        "ssid": "Workshop-WiFi",
        "ip": "192.168.1.42",
        "mdns": "http://vibemonitor.local",
        "rssi": -58,
    }


class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt: str, *args) -> None:
        print("%s - %s" % (self.address_string(), fmt % args))

    def _send(self, code: int, body: bytes, content_type: str) -> None:
        self.send_response(code)
        self.send_header("Content-Type", content_type)
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self) -> None:
        path = urlparse(self.path).path
        if path in ("/", "/index.html"):
            self._send(200, DASHBOARD.encode(), "text/html; charset=utf-8")
        elif path == "/wifi":
            self._send(200, WIZARD.encode(), "text/html; charset=utf-8")
        elif path in ("/connecting", "/wifi/save"):
            self._send(200, CONNECTING.encode(), "text/html; charset=utf-8")
        elif path == "/api/status":
            self._send(200, json.dumps(mock_status()).encode(), "application/json")
        elif path == "/api/scan":
            nets = [
                {"ssid": "Workshop-WiFi", "rssi": -48, "secure": True},
                {"ssid": "Guest", "rssi": -67, "secure": False},
                {"ssid": "Shop-5G", "rssi": -72, "secure": True},
            ]
            self._send(200, json.dumps(nets).encode(), "application/json")
        else:
            self._send(404, b"not found", "text/plain")

    def do_POST(self) -> None:
        global sensitivity, avg_window, ntfy_topic
        parsed = urlparse(self.path)
        if parsed.path == "/api/sensitivity":
            qs = parse_qs(parsed.query)
            if "value" in qs:
                sensitivity = max(10, min(100, int(qs["value"][0])))
            self._send(200, b"ok", "text/plain")
            return
        if parsed.path == "/api/avgwindow":
            qs = parse_qs(parsed.query)
            if "value" in qs:
                avg_window = max(0.0, min(10.0, float(qs["value"][0])))
            self._send(200, b"ok", "text/plain")
            return
        if parsed.path == "/api/ntfy":
            qs = parse_qs(parsed.query)
            raw = qs.get("topic", [""])[0]
            ntfy_topic = "".join(
                c for c in raw if c.isalnum() or c in "_-"
            )[:64]
            self._send(200, b'{"ok":true}', "application/json")
            return
        if parsed.path == "/api/ntfy-test":
            if not ntfy_topic:
                self._send(400, b'{"ok":false,"err":"no topic"}', "application/json")
                return
            self._send(200, b'{"ok":true}', "application/json")
            return
        if parsed.path == "/wifi/save":
            self._send(200, CONNECTING.encode(), "text/html; charset=utf-8")
            return
        self._send(404, b"not found", "text/plain")


def main() -> None:
    server = ThreadingHTTPServer(("127.0.0.1", 8080), Handler)
    print("Preview http://127.0.0.1:8080  (wizard http://127.0.0.1:8080/wifi)")
    server.serve_forever()


if __name__ == "__main__":
    main()
