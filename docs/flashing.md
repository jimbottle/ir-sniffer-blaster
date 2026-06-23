# Flashing

ESPHome compiles a per-device firmware from the YAML config and flashes it. There
are two ways to run ESPHome:

- **Home Assistant ESPHome add-on** (GUI), or
- **Standalone CLI** — `pip install esphome` (see [`requirements.txt`](../requirements.txt))
  or the official Docker image `ghcr.io/esphome/esphome`.

## Prerequisites

- Hardware wired per [`docs/wiring.md`](wiring.md).
- `secrets.yaml` created from the template and filled in — all five keys:
  `wifi_ssid`, `wifi_password`, `api_encryption_key`, `ota_password`, `ap_password`:
  ```bash
  cp secrets.yaml.example secrets.yaml
  # edit it; generate the API key with:
  openssl rand -base64 32
  ```
- A **USB-C DATA cable** for the first flash. The included 4298 PSU is power-only
  and **cannot** flash the board.

## First flash (over USB)

1. Place `ir-blaster-livingroom.yaml` and `secrets.yaml` together (they already are
   in this repo root).
2. Connect the Feather to the computer with the **USB-C data cable**.
3. Compile + flash:
   ```bash
   esphome run ir-blaster-livingroom.yaml
   ```
   Pick the serial port when prompted. On macOS it appears as
   `/dev/cu.usbserial-*` or `/dev/cu.usbmodem*` (not `/dev/ttyUSB*` — that's Linux).
   *(HA add-on: install via "Plug into this computer".)*
4. Confirm it joins WiFi and appears in Home Assistant / responds at
   `http://192.168.0.150/`.

## Later updates (over the air)

After the first successful flash, **USB is no longer needed** — the 4298 PSU just
powers the board:

```bash
esphome run ir-blaster-livingroom.yaml
# choose the OTA / network target instead of the serial port
```

OTA is gated by `ota_password` from `secrets.yaml`.

## Docker alternative

```bash
docker run --rm -v "${PWD}":/config -it ghcr.io/esphome/esphome run ir-blaster-livingroom.yaml
```

For USB flashing inside Docker you must pass the serial device through
(`--device=/dev/ttyUSB0` or similar); OTA flashes need no device passthrough.

> **macOS note:** Docker Desktop runs in a VM and generally **cannot** pass
> through a USB serial device, so the Docker route won't work for the *first*
> (USB) flash on a Mac — use the native CLI (`esphome run`) for that. Docker is
> fine for later **OTA** flashes, which go over the network.
