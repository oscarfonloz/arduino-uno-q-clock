# Arduino UNO Q — Scrolling Clock + Live USD/MXN Exchange Rate

A real-time scrolling display for the **Arduino UNO Q** that shows the current time and live USD → MXN exchange rate on the built-in 8×13 LED matrix, with a Modulino Pixels module lighting up today's day of the week.

![Arduino UNO Q](https://www.arduino.cc/wiki/static/e9424f1e5a1ab0ec6d6b6e6daf2e3aae/arduino-uno-q.jpg)

---

## What It Does

- Scrolls `10:28 PM   |   $1 = 17.34 MXN` continuously across the LED matrix
- Updates the exchange rate every 30 minutes (free API, no key required)
- Lights one of 7 Modulino Pixels LEDs to indicate today's day of the week (Mon–Sun)
- Timezone: **Central Time (America/Chicago)** — auto-handles CST/CDT
- Exposes the same message through a WebUI on the local network

---

## Hardware Required

| Component | Notes |
|---|---|
| Arduino UNO Q | 2GB or 4GB variant |
| Modulino Pixels | Connected via Qwiic/I2C |
| USB-C cable | Power + programming |

---

## How It Works

The UNO Q has two processors that work together:

```
┌─────────────────────────────────┐     Bridge.call("setDisplay", ...)
│  Qualcomm QRB2210 (Linux side)  │ ──────────────────────────────────►  ┌──────────────────────────┐
│  clock_exchange.py              │                                       │  STM32U585 (MCU side)    │
│  - Gets current time (CST/CDT)  │ ◄──────────────────────────────────  │  sketch.ino              │
│  - Fetches USD/MXN rate via API │     Bridge.provide("setDisplay")      │  - Scrolls LED matrix    │
│  - Sends string every second    │                                       │  - Updates Modulino LEDs │
└─────────────────────────────────┘                                       └──────────────────────────┘
```

The Linux side handles all internet/API work. The STM32 handles all real-time display rendering. They communicate via the **RouterBridge RPC system** — the STM32 registers a function (`Bridge.provide`), and Python calls it (`Bridge.call`).

---

## Setup & Deployment

### 1. Libraries (install via App Lab → Sketch Libraries)

- `Arduino_RouterBridge` — bridge between Linux and STM32
- `Modulino` — Modulino Pixels driver

> **Note:** `Arduino_LED_Matrix` and `MD_Parola` are **not used**. The built-in matrix is driven directly via low-level Zephyr C functions (`matrixBegin` / `matrixWrite`). MD_Parola has a known linker error on the UNO Q's Zephyr core — avoid it.

### 2. Python dependency

Open the App Lab terminal (`>_` icon) and run:

```bash
pip3 install requests
```

(`zoneinfo` is built into Python 3.9+ and already present on the board.)

### 3. Deploy

1. Paste `sketch.ino` into the C++ editor in App Lab
2. Paste `clock_exchange.py` into the Python editor
3. Click **Run**

The STM32 will show `Loading...` on the matrix until the Python side completes its handshake and sends the first display string (usually within 2–3 seconds).

### 4. Display on the screen

You can access the local website through http://IP:7000/ per Arduino's documentation of the WebUI brick. Where IP is the IP of your Arduino device.

To use an HDMI display, I recommend the following:
1. Disable the login screen so the device logs into the user directly at boot
```apache
sudo mkdir -p /etc/lightdm/lightdm.conf.d
sudo tee /etc/lightdm/lightdm.conf.d/50-autologin.conf <<'EOF'
[Seat:*]
autologin-user=arduino
autologin-user-timeout=0
EOF
sudo reboot
```
1. Disable the X11 monitor off settings and screensaver
    1. `sudo nano /etc/X11/xorg.conf.d/99-disable-dpms.conf`
    1. Add the following: 
    ```apache
    Section "ServerFlags"
        Option "BlankTime" "0"
        Option "StandbyTime" "0"
        Option "SuspendTime" "0"
        Option "OffTime" "0"
    EndSection
    ```
    1. `sudo reboot`
1. Disable the Arduino App Lab automatic launch (not needed but is less screen clutter)
```apache
mkdir -p ~/.config/autostart
cp /etc/xdg/autostart/ArduinoAppLab.desktop ~/.config/autostart/
sed -i '$a Hidden=true' ~/.config/autostart/ArduinoAppLab.desktop
```
1. Run `DISPLAY=:0 chromium --kiosk --noerrdialogs --disable-infobars http://localhost:7000/` from SSH

---

## Configuration

All user-adjustable values are at the top of each file:

**`clock_exchange.py`**
```python
EXCHANGE_REFRESH_MINUTES = 30          # How often to re-fetch the rate
TIMEZONE = ZoneInfo("America/Chicago") # Change to your timezone
```

**`sketch.ino`**
```cpp
#define SCROLL_STEP_MS 80   // Scroll speed — lower = faster
const int DIM_BR    = 3;    // Modulino brightness for inactive days (0-100)
const int ACTIVE_BR = 25;   // Modulino brightness for today (0-100)
```

---

## Exchange Rate API

Uses [Frankfurter.app](https://www.frankfurter.app) — free, no API key, no sign-up required.

```
GET https://api.frankfurter.app/latest?from=USD&to=MXN
```

---

## Acknowledgements

Built with [Frankfurter.app](https://www.frankfurter.app) for exchange rate data.  
Developed on Arduino UNO Q firmware v0.55.2 / App Lab v0.10.0 (August 2026).
Developed with extensive help from Claude.