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
- If the button bothers the readings, move the piezo to another analog pin and change `PIEZO_PIN`. Do not use D10 — that is the calibration trigger.

## Arduino IDE setup

1. Install [Arduino IDE](https://www.arduino.cc/en/software).
2. Boards Manager URL (File → Preferences):

   `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`

3. Install **esp32** by Espressif, then select board **XIAO_ESP32C3** (or **Seeed XIAO ESP32C3**).
4. Tools → **USB CDC On Boot: Enabled**.
5. Library Manager: install **U8g2** by oliver.
6. Open `xiao_vibration_monitor.ino` and upload. The sketch is self-contained (HTML is inlined). `web_pages.h` is an optional copy of those pages and is **not** required to compile.

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

The page shows live amplitude (0–100), a DETECTED / QUIET badge, a short history graph, a sensitivity slider, and an **average window** slider (0–10 seconds). 0 s is instant; longer windows smooth the OLED and phone percent.

## Using it later

- Same Wi-Fi: just power on and open the IP or `http://vibemonitor.local`.
- New location: hold **USER** while resetting, or open `/wifi` on the dashboard, or type `w` in the Serial Monitor (115200 baud).
- Type `c` in Serial Monitor to erase the saved network.
- Type `k` in Serial Monitor to start dryer calibration.

## Dryer ON/OFF calibration

The piezo should be firmly attached to the dryer housing. Calibration learns the vibration of **running** vs **stopped** and stores it in flash until you run it again.

Do not use **D0** for the cal button. D0 is GPIO2 (ADC1), and piezo sampling on D1 can make D0 read LOW.

1. Wire a momentary button or jumper from **D10** to **GND** (MOSI pad; leave the expansion SD slot empty).
2. With the sketch running, **hold D10 low** for about one second.
3. OLED: **START dryer NOW** — start it immediately. The countdown is time for the drum to be running before sampling. Keep it tumbling through **Keep dryer ON / sampling**.
4. OLED: **STOP dryer NOW** — stop it immediately and let it settle through the countdown, then **Keep dryer OFF / sampling**.
5. OLED: **Calibration Complete**. Thresholds are saved.

If you wait until the countdown hits 0 to start or stop the dryer, that transition gets mixed into the sample and ON/OFF will be hard to tell apart.

**Sensitivity** and **Average window** do not change the numbers stored during calibration (those use raw piezo peak-to-peak). After calibration, ON/OFF detection also ignores those sliders. They only affect the live 0–100% display.

After a good cal the OLED shows **DRYER ON** or **DRYER OFF** (**cal needed** until the first run). Hold D10 low again to recapture. Serial `k` starts the same routine.

## iPhone alerts (ntfy)

The board can push a notification when the calibrated dryer state stays ON or OFF for about 4 seconds after a change. It uses the free [ntfy](https://ntfy.sh) service — no Apple developer account, and nothing has to stay open in Safari.

1. On the iPhone, install **ntfy** from the App Store.
2. In the app, subscribe to a **private topic name** (treat it like a password: letters, numbers, `_`, `-` only; anyone who knows the name can send to it).
3. Put the XIAO on your home Wi-Fi (not the `VibeMonitor` setup AP — that has no internet).
4. Open the dashboard (`http://vibemonitor.local` or the IP on the OLED).
5. Under **iPhone alerts**, enter the same topic, tap **Save topic**, then **Send test alert**.

You should get “VibeMonitor test”. After that, “Dryer ON” / “Dryer OFF” fire on real state changes. Power-on and a fresh calibration do **not** send an alert for the first stable reading. Leave the topic blank and save to disable.

The HTTPS POST to `ntfy.sh` takes a moment; the OLED may pause briefly when an alert goes out.

## OLED

| Screen | Meaning |
| --- | --- |
| Wi-Fi setup | Join AP `VibeMonitor`, then open `192.168.4.1` |
| Connecting | Joining the saved network |
| DRYER ON/OFF + bar + IP | Calibrated machine state and live % |
| cal needed | Run the D10 calibration routine |

If the OLED is upside down, change `u8g2.setFlipMode(1)` to `0` in `setup()`.

## Amplitude

The sketch measures **peak-to-peak** over a short window, subtracts a slow noise floor, and scales the result to 0–100. It is a **relative** knock/vibration level, not a calibrated g or dB reading. Use the phone **Sensitivity** slider if idle noise triggers DETECTED, or if real knocks look too small.

## Serial commands

| Key | Action |
| --- | --- |
| `w` | Open the Wi-Fi wizard |
| `c` | Forget saved SSID/password |
| `k` | Start dryer ON/OFF calibration |
