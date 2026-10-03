# Web Installer Setup

This package is prepared so GitHub Actions can:

1. compile the CYD firmware,
2. create a single merged ESP32 binary,
3. deploy the browser installer through GitHub Pages.

## What remains

Then in GitHub:

- Settings → Pages
- Source: **GitHub Actions**

Push to `main` or run the workflow manually.

When the workflow succeeds, GitHub Pages will provide a URL.
Open that URL in Chrome/Edge and press **Install Universal Desk Dashboard**.

## Important

The firmware has not yet been compiled/tested on your physical CYD.
The first GitHub Actions build will also validate whether the current source
compiles cleanly against the current ESP32 core / LovyanGFX / ArduinoJson versions.
