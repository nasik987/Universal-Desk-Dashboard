# Universal Desk Dashboard v0.3

Firmware for the **CYD2USB ESP32-2432S028R, ST7789, 320 × 240, XPT2046 touch**. Preserves the working display inversion, touch calibration and SPI pin configuration.

## Display design

The default theme uses near-black backgrounds, charcoal cards, soft-white text and blue accents. `DeskTheme.h` shares logical RGB565 colors between the main renderer, Wi-Fi setup screens and startup messages. The display's verified inversion is preserved; colors are complemented once at the hardware boundary.

Inter is rasterized directly at each native pixel size with antialiasing. Rounded line icons use the same stroke weight. No text or icons are enlarged from smaller bitmaps. `DeskUI.h` is the shared production renderer, used by both the ESP32 and `tests/render_ui.cpp`; [the software preview](design/UI-preview.png) uses sample screen data. Missing-data and long-text states are checked for pixel overflow.

Completed 80-row strips are transferred to the screen to avoid full-screen clearing flashes while using about 50 KiB of framebuffer memory. If that allocation fails, drawing falls back to the screen. Tests compare striped rendering to full-frame rendering.

Assets can be regenerated with `python scripts/generate_assets.py --font Inter.ttf` (Pillow + CairoSVG). Font source: [Inter from Google Fonts](https://github.com/google/fonts/tree/main/ofl/inter), under SIL OFL; license in `design/Inter-OFL.txt`. Icons are custom SVG paths stored in the generator.

## Screens

- Clock: large modern clock, small date, current weather. Tap the clock to open apps.
- Apps: four large line icons for weather, Focus, MakerWorld and settings.
- Weather: next six hourly forecasts, rain probability, five-day forecast, sunrise/sunset and wind. Tap the bottom-right switch for hourly / daily view.
- Focus: 25 / 5 / 15 minute presets, start, pause and reset. Completion works even when another page is open.
- Stopwatch: tap the top-right button on Focus to switch modes. Start, pause, resume and reset with the bottom controls. Shows hundredths, then hours for longer sessions; continues while another screen or Focus is open. Reset stops the stopwatch, and a device restart clears it.
- MakerWorld: prints, downloads, likes and followers; graph of the last seven available print-total measurements, with actual measurement timestamp.
- Settings: six buttons for Wi-Fi, Graphics, Display, Time, Weather and web setup.

## On-device settings

- Graphics: switch Dark / Light and choose Blue, Mint, Purple, Orange or White (Gray in the light theme). Buttons and icons update immediately.
- Display: tap either brightness slider to choose a level. Night dim is optional; tap the start/end hour buttons to cycle the schedule. A schedule can cross midnight; identical start/end hours disable the interval. Night brightness never raises the normal brightness. Until network time is available, normal brightness applies.
- Time: choose 12 / 24 hours and cycle Central EU, UTC, London or New York. Twelve-hour clocks show AM/PM. Existing custom time zones remain active until changed.
- Weather: cycle Brno, Praha, Ostrava, Plzeň or Bratislava and request a refresh. Existing custom locations remain active until changed. Location changes discard measurements for the previous city and request new weather in the background.

Changes save automatically to Preferences and survive restart. The existing web form remains available for custom locations, time-zone rules and MakerWorld URLs. Back returns from these pages to Settings. [Settings preview](design/Settings-preview.png) uses the same renderer as the firmware.

Swipe left/right between all six screens, or tap the page dots. Swipes are recognized on release so crossing a button cannot activate it. Wi-Fi keyboard remains a separate setup flow.

## Data and reliability

Weather uses [Open-Meteo](https://open-meteo.com/en/docs), refreshed every 15 minutes. MakerWorld reads public JSON every 30 minutes. Requests run in a background task; display and timer continue responding. Failures retry after two minutes and retain the last valid reading. Responses are capped at 64 KiB.

Default MakerWorld source is `nasik987/nasik-makerworld-monitor/data/history.json`. Its last measurement was **2026-09-23** when this version was developed. This firmware does not restart the collector or invent current statistics. Measurements over 36 hours old are labelled **OLD DATA**. Missing metrics appear as `--`. The graph shows recent measurements, not a fabricated seven-day history or today's gains.

Set a public HTTPS stats URL through the device web setup. Supported JSON is the existing monitor history array, an object with `history`, or a single snapshot:

```json
{
  "time_utc": "2026-10-03T23:00:00Z",
  "metrics": {
    "followerCount": 3511,
    "printCount": 45480,
    "downloadCount": 94514,
    "likeCount": 30474
  }
}
```

Example numbers above illustrate the schema only. A flat object with `timestamp`, `followers`, `prints`, `downloads`, `likes` is also supported. Times require an explicit UTC `Z` or offset. MakerWorld login cookies are not stored on the display.

## Install / configure

Use [the USB web installer](https://nasik987.github.io/Universal-Desk-Dashboard/) in Chrome or Edge. Choose installation without erasing device settings when updating. First boot shows Wi-Fi selection; setup AP is `DeskDash-Setup`, web address `http://192.168.4.1`. Once connected, settings show the device LAN address. Default location is Brno.

Libraries: **TFT_eSPI**, **XPT2046_Touchscreen**, **ArduinoJson 7.x**; ESP32 Arduino core **3.1.3**. Copy `User_Setup_CYD2USB.h` to TFT_eSPI's `User_Setup.h` before compiling. Board: `esp32:esp32:jczn_2432s028r`. CI builds and publishes the USB installer.

Local data tests:

```sh
g++ -std=c++17 -I /path/to/ArduinoJson/src -I UniversalDeskDashboard tests/data_test.cpp -o /tmp/dashboard-data-test
/tmp/dashboard-data-test
```

The hardware must still be checked after flashing, particularly swipe sensitivity, inverted panel colors and text at 320 × 240. HTTPS currently retains the existing project's `setInsecure()` transport configuration; only public weather/statistics feeds are fetched.
