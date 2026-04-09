# WordyWatch Web Flasher

A browser-based firmware flasher for the WordyWatch (ESP32-C6) using [ESP Web Tools](https://esphome.github.io/esp-web-tools/) and WebSerial.

## How it works

The flasher is a single static HTML page hosted on GitHub Pages. When a user clicks the install button, the browser connects to the watch over USB-C using the WebSerial API and flashes the firmware directly — no tools or drivers needed.

## Deployed URL

After enabling GitHub Pages, the flasher is available at:

```
https://robdobsn.github.io/WordyWatch/
```

## Build & deployment

The GitHub Actions workflow (`.github/workflows/deploy-flasher.yml`) runs automatically when a new release is published:

1. Builds the firmware inside the `espressif/idf:v5.5.2` Docker container using `idf.py -B build/WordyWatchV11 build`
2. Copies the three flash binaries (bootloader, partition table, application) into the site
3. Generates `manifest.json` with the release version tag
4. Deploys everything to GitHub Pages

The workflow can also be triggered manually from the Actions tab using `workflow_dispatch`.

## GitHub Pages setup

To enable GitHub Pages for this repo:

1. Go to **Settings → Pages** in the GitHub repository
2. Under **Build and deployment → Source**, select **GitHub Actions**

That's all — the workflow handles the rest.

## Flash layout

| Binary | Offset | Description |
|---|---|---|
| `bootloader.bin` | `0x0` | ESP32-C6 second-stage bootloader |
| `partition-table.bin` | `0x8000` | Partition table |
| `WordyWatchV11.bin` | `0x20000` | Application firmware |

## Browser support

WebSerial is required, which is supported in:

- Google Chrome (desktop)
- Microsoft Edge (desktop)

Firefox, Safari, and mobile browsers are **not supported**. The page shows a clear message to users on unsupported browsers.

## Local development

To preview the flasher page locally:

```bash
cd flasher
python3 -m http.server 8000
```

Then open `http://localhost:8000`. Note that WebSerial requires HTTPS in production, but works on `localhost` for testing.

## Files

| File | Purpose |
|---|---|
| `index.html` | Flasher page with ESP Web Tools install button |
| `firmware/manifest.json` | Template manifest (overwritten by CI with actual version) |
