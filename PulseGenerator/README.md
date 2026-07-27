# XIAO ESP32S3 Pulse Generator

Arduino sketch for the **Seeed XIAO ESP32S3**. It drives **1 ms pulses** on pin **D10** at a frequency you set from a smartphone (1–500 Hz). Run length is also set from the phone (1–60 minutes).

A **WiFi connection wizard** (captive portal) walks you through linking the phone and the XIAO.

## What you get

| Setting | Range | Notes |
|--------|--------|--------|
| Frequency | 1–500 Hz | Set with on-screen slider |
| Pulse width | 1 ms | Fixed |
| Duration | 1–60 min | Set with on-screen slider |
| Output pin | D10 (GPIO9) | Active-high pulses |

## Arduino IDE setup

1. Install the **ESP32** board package (Espressif Systems) via Boards Manager.
2. Select board: **XIAO_ESP32S3** (Seeed Studio).
3. Open `PulseGenerator/PulseGenerator.ino` and upload.

## WiFi connection wizard

1. Power the XIAO. It creates Wi‑Fi network **`XIAO-PulseGen`** (password **`pulse1234`**).
2. On your phone, join that network.
3. A captive-portal / “Sign in to network” banner should open the **WiFi Setup Wizard**.  
   If it does not, open **`http://192.168.4.1`** in the browser.
4. Choose one path:
   - **Use device hotspot** — stay on `XIAO-PulseGen`, then open pulse controls.
   - **Join home Wi‑Fi** — scan networks, enter the password; the XIAO joins your router. Switch your phone to the same home Wi‑Fi and open the IP shown (also linked from the wizard).
5. Use the frequency / duration sliders and **Start**.

Saved home Wi‑Fi credentials are stored in flash and reused on the next boot. The setup hotspot stays available so you can run the wizard again from **Wi‑Fi setup wizard** on the control page, or open `http://192.168.4.1/wizard`.

Serial Monitor at **115200 baud** prints SoftAP / STA status after boot.

## Customizing SoftAP Wi‑Fi

Edit these constants near the top of the sketch:

```cpp
static const char* AP_SSID     = "XIAO-PulseGen";
static const char* AP_PASSWORD = "pulse1234";
```

## Timing notes

- Pulses are generated with ESP32 `esp_timer` one-shots so the web server does not block edges.
- At 500 Hz the period is 2 ms (1 ms high / 1 ms low). At lower frequencies the high time stays 1 ms and the low time fills the rest of the period.
- Duration is wall-clock minutes from Start; when it expires the pin is forced low.
