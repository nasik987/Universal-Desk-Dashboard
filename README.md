# Universal Desk Dashboard v0.1

First MVP for **ESP32-2432S028 / CYD (Cheap Yellow Display)**.

## What works in v0.1
- Home screen with time/date, weather and focus timer
- Current weather via Open-Meteo (no API key)
- 25 / 5 / 15 minute Pomodoro presets
- Touch navigation
- Web setup portal
- Wi-Fi, city, coordinates, timezone and custom-link settings
- Persistent settings in ESP32 Preferences
- NTP time
- Automatic setup AP when Wi-Fi is missing or fails

## Arduino IDE libraries
Install:
1. ESP32 board package
2. LovyanGFX
3. ArduinoJson 7.x

## First boot
1. Flash `UniversalDeskDashboard.ino`.
2. Connect your phone to Wi-Fi `DeskDash-Setup`.
3. Open `http://192.168.4.1`.
4. Enter Wi-Fi and location settings.
5. Save; the CYD restarts.

## Default location
Brno, Czechia (`49.1951, 16.6068`).

## v0.2 plan
- Wi-Fi scanning setup wizard
- weather icons + 3-day forecast
- QR widget
- reusable widget framework
- custom JSON API widget
- themes and widget ordering
- OTA and web installer

Note: this source has not yet been hardware-compiled/tested in this environment. The CYD display/touch pin map is based on the working DataDisplay project you referenced.
