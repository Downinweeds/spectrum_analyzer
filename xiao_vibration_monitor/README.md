# XIAO ESP32-C3 piezo vibration monitor

Arduino sketch for a **Seeed Studio XIAO ESP32-C3** on the **XIAO Expansion Board** (0.96" OLED). A piezo on **D1** is sampled for vibration, the relative level is drawn on the OLED, and the same live reading is served to your phone over Wi-Fi.

A **Wi-Fi setup wizard** (captive portal) runs whenever there is no saved network, the join fails, or you ask for it. Credentials are stored in flash, so you can move the board to a new location and set Wi-Fi from your phone.

## What you need

- Seeed XIAO ESP32-C3 seated on the Expansion Board
- Piezo disc / vibration sensor
- **1 MΩ resistor** across the piezo leads (required)
- Optional: 3.3 V zener or 1 kΩ series resistor if the disc is large (piezos can spike above 3.3 V)

## Wiring

```
Piezo +  ----  XIAO D1  (also labeled A1, GPIO3)
Piezo -  ----  GND
1 MΩ     ----  between D1 and GND (across the piezo)
```

A 3-wire analog vibration module also works: **SIG → D1**, **VCC → 3V3**, **GND → GND**.

D1 is an ADC1 pin, so analog reads still work while Wi-Fi is on.

### D1 is also the expansion USER button

On the Expansion Board the **USER button shares D1**. That is fine for this sketch:

- Do not hold the USER button while you are measuring vibration.
- **Hold USER while you tap RESET** if you want to force the Wi-Fi wizard (for example after moving to a new network).
- If the button bothers the readings, move the piezo to the Grove **A0/D0** connector and change `PIEZO_PIN` in the sketch from `D1` to `D0`.

## Arduino IDE setup

1. Install [Arduino IDE](https://www.arduino.cc/en/software).
2. Boards Manager URL (File → Preferences):

   `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`

3. Install **esp32** by Espressif, then select board **XIAO_ESP32C3** (or **Seeed XIAO ESP32C3**).
4. Tools → **USB CDC On Boot: Enabled**.
5. Library Manager: install **U8g2** by oliver.
6. Put **both** `xiao_vibration_monitor.ino` and `web_pages.h` in the same sketch folder, then open the `.ino` and upload.

The XIAO ESP32-C3 needs its **external antenna plugged into the U.FL connector**. Without it, Wi-Fi setup can stall.

After upload the OLED should move past `starting...` to **Wi-Fi setup** (join phone network `VibeMonitor`) or **Connecting**. If it stays on `starting...`, close Arduino and reopen the sketch after a GitHub Desktop pull so you are not compiling an old copy.

## First-time Wi-Fi wizard (phone)

1. Power the board. The OLED shows **Wi-Fi setup**.
2. On your phone, join the open network **VibeMonitor**.
3. A setup page should appear. If it does not, open `http://192.168.4.1`.
4. Tap your home/shop Wi-Fi, enter the password, tap **Connect**.
5. Leave the **VibeMonitor** network and rejoin your normal Wi-Fi.
6. On the OLED, read the IP address (for example `192.168.1.42`).
7. On the phone browser open that IP, or try `http://vibemonitor.local`.

The page shows live amplitude (0–100), a DETECTED / QUIET badge, a short history graph, and a sensitivity slider.

## Using it later

- Same Wi-Fi: just power on and open the IP or `http://vibemonitor.local`.
- New location: hold **USER** while resetting, or open `/wifi` on the dashboard, or type `w` in the Serial Monitor (115200 baud).
- Type `c` in Serial Monitor to erase the saved network.

## OLED

| Screen | Meaning |
| --- | --- |
| Wi-Fi setup | Join AP `VibeMonitor`, then open `192.168.4.1` |
| Connecting | Joining the saved network |
| VIBRATION + bar + IP | Live relative amplitude (0–100%) |

If the OLED is upside down, change `u8g2.setFlipMode(1)` to `0` in `setup()`.

## Amplitude

The sketch measures **peak-to-peak** over a short window, subtracts a slow noise floor, and scales the result to 0–100. It is a **relative** knock/vibration level, not a calibrated g or dB reading. Use the phone **Sensitivity** slider if idle noise triggers DETECTED, or if real knocks look too small.

## Serial commands

| Key | Action |
| --- | --- |
| `w` | Open the Wi-Fi wizard |
| `c` | Forget saved SSID/password |
