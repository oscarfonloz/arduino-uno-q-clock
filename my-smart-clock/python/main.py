"""
clock_exchange.py  —  Qualcomm / Linux side
Arduino UNO Q — Clock + USD/MXN exchange rate

- Sends scrolling display string to STM32 every second
- Sends day-of-week so STM32 can light the Modulino Pixels
- Timezone: America/Chicago (Central Time — handles CST/CDT automatically)
"""

import time
import requests
from datetime import datetime
from zoneinfo import ZoneInfo          # built-in since Python 3.9 (Debian has it)
from arduino.app_utils import App, Bridge

# ─── Configuration ────────────────────────────────────────────────────────────
EXCHANGE_REFRESH_MINUTES = 30
API_URL   = "https://api.frankfurter.app/latest?from=USD&to=MXN"
TIMEZONE  = ZoneInfo("America/Chicago")  # Central Time — auto CST/CDT

# ─── State ────────────────────────────────────────────────────────────────────
exchange_rate      = None
last_exchange_fetch = 0


# ─── Handshake ────────────────────────────────────────────────────────────────
def linux_started():
    print("STM32 handshake — Python ready!")
    return True

Bridge.provide("linux_started", linux_started)


# ─── Helpers ──────────────────────────────────────────────────────────────────
def fetch_exchange_rate():
    try:
        r = requests.get(API_URL, timeout=5)
        rate = r.json()["rates"]["MXN"]
        return f"{rate:.2f}"
    except Exception as e:
        print(f"Exchange fetch failed: {e}")
        return None


def build_display_string(rate_str, now):
    time_str = now.strftime("%I:%M %p").lstrip("0")   # e.g. "10:28 PM"
    if rate_str:
        return f"{time_str}   |   $1 = {rate_str} MXN   "
    else:
        return f"{time_str}   |   Rate unavailable   "


# ─── Main loop ────────────────────────────────────────────────────────────────
def loop():
    global exchange_rate, last_exchange_fetch

    now    = datetime.now(TIMEZONE)
    now_ts = time.time()

    # day_of_week: 0=Mon ... 6=Sun (matches Python weekday())
    day_of_week = now.weekday()

    # Refresh exchange rate every 30 minutes
    if exchange_rate is None or (now_ts - last_exchange_fetch) >= (EXCHANGE_REFRESH_MINUTES * 60):
        print("Fetching exchange rate...")
        new_rate = fetch_exchange_rate()
        if new_rate:
            exchange_rate      = new_rate
            last_exchange_fetch = now_ts
            print(f"Rate updated: {exchange_rate}")

    display = build_display_string(exchange_rate, now)
    # Bridge.call() only supports one parameter — pack both into one string
    # Format: "10:28 PM   |   $1 = 17.34 MXN   ;4"  (semicolon = separator)
    payload = f"{display};{day_of_week}"
    Bridge.call("setDisplay", payload)
    print(f"Sent: {payload}")

    time.sleep(1)


App.run(user_loop=loop)