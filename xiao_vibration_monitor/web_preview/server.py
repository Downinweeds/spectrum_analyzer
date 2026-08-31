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
t0 = time.time()
hist: list[float] = []


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
    elapsed = time.time() - t0
    # Quiet most of the time, with periodic knocks.
    knock = max(0.0, math.sin(elapsed * 0.9)) ** 6 * 82
    chatter = 3 + 2 * abs(math.sin(elapsed * 7.0))
    raw = int(40 + chatter * 3 + knock * 8)
    amplitude = int(min(100, chatter + knock * (sensitivity / 50.0)))
    hist.append(float(amplitude))
    if len(hist) > 400:
        del hist[: len(hist) - 400]
    n = max(1, int(round(avg_window / 0.25))) if avg_window > 0 else 1
    if avg_window <= 0:
        smoothed = amplitude
    else:
        window = hist[-n:]
        smoothed = int(round(sum(window) / len(window)))
    return {
        "amplitude": smoothed,
        "peak": max(smoothed, int(knock + 18)),
        "raw": raw,
        "detected": smoothed >= 12,
        "sensitivity": sensitivity,
        "avgWindow": round(avg_window, 1),
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
        global sensitivity, avg_window
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
