# XIAO ESP32S3 Pulse Generator

Arduino sketch for the **Seeed XIAO ESP32S3**. It drives **1 ms pulses** on pin **D10** at a frequency you set from a smartphone (1–500 Hz). Run length is also set from the phone (1–60 minutes).

## What you get

| Setting | Range | Notes |
|--------|--------|--------|
| Frequency | 1–500 Hz | Set with on-screen slider |
| Pulse width | 1 ms | Fixed |
| Duration | 1–60 min | Set with on-screen slider |
| Output pin | D10 (GPIO9) | Active-high pulses |

The board creates its own Wi‑Fi access point and serves a mobile web UI with graphic sliders, Start/Stop, and live remaining-time status.

## Arduino IDE setup

1. Install the **ESP32** board package (Espressif Systems) via Boards Manager.
2. Select board: **XIAO_ESP32S3** (Seeed Studio).
3. Open `PulseGenerator/PulseGenerator.ino` and upload.

## Phone setup

1. Join Wi‑Fi network **`XIAO-PulseGen`** (password: **`pulse1234`**).
2. Open **`http://192.168.4.1`** in the phone browser.
3. Move the **Frequency** and **Duration** sliders, then tap **Start**.
4. Tap **Stop** anytime to end the pulse train early.

Serial Monitor at **115200 baud** prints the SoftAP status and IP after boot.

## Timing notes

- Pulses are generated with ESP32 `esp_timer` one-shots so the web server does not block edges.
- At 500 Hz the period is 2 ms (1 ms high / 1 ms low). At lower frequencies the high time stays 1 ms and the low time fills the rest of the period.
- Duration is wall-clock minutes from Start; when it expires the pin is forced low.

## Customizing Wi‑Fi

Edit these constants near the top of the sketch:

```cpp
static const char* AP_SSID     = "XIAO-PulseGen";
static const char* AP_PASSWORD = "pulse1234";
```
