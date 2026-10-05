# Hooky – Pebble Webhook Trigger App

Hooky is a modern Pebble app built using the **Pebble SDK 2025**, designed to trigger custom **HTTP webhooks** directly from your Pebble smartwatch — via your connected smartphone’s internet connection.  
It’s the perfect lightweight automation companion for webhooks, IFTTT, Home Assistant, or any REST API you want to trigger with a single tap.

---

## Features

- **One-tap webhook trigger** – send an HTTP POST request to your configured endpoint.
- **Smartphone bridge** – uses PebbleKit JS to connect via your phone’s network.
- **Customizable endpoint** – define your own webhook URL in the Pebble app settings.
- **Offline-safe** – the watch UI works even if the network connection is temporarily unavailable.
- **Lightweight** – minimal code footprint and optimized for modern Pebble SDK 2025.
- **Secure** - Uses CloudFalre Zero Trust Authentication Tokens to connect securly to the infrastructure

---

## Supported watches

Hooky is built for every platform of the current Core Devices Pebble SDK:

| Platform | Watches | Display | Notes |
|----------|---------|---------|-------|
| `emery`   | Pebble Time 2 | 200×228, color | Touch controls, sound feedback |
| `gabbro`  | Pebble Round 2 | 260×260 round, color | Touch controls |
| `flint`   | Pebble 2 Duo | 144×168, black & white | Sound feedback |
| `basalt`  | Pebble Time, Time Steel | 144×168, color | |
| `chalk`   | Pebble Time Round | 180×180 round, color | |
| `diorite` | Pebble 2 (SE, HR) | 144×168, black & white | |
| `aplite`  | Pebble, Pebble Steel | 144×168, black & white | |

- **Touch controls** (scrolling and tap-to-run, touch buttons in the confirmation dialog) are used on touchscreen watches and can be switched off in the settings.
- **Sound feedback** plays on watches with a speaker; other watches ignore the setting.
- Row heights follow each watch's system text size, so names and descriptions are never cut off on the larger displays.

## Show the response on the watch

Each webhook can show its answer instead of just "Success" (settings → webhook → **Response**):

| Show on watch | Use it for | Example |
|---|---|---|
| Status only | Actions (default) | "Success" |
| Response text | APIs that answer with plain text (HTML is reduced to text) | `Garage: open` |
| Value from a JSON response | Pick one value with a path like `state`, `attributes.temperature` or `items[0].name` | `21.5` |

The optional **Format** wraps the value, e.g. `{value} °C` or `Garage: {value}` (`\n` starts a new line).
The answer (up to ~480 characters) opens in a window on the watch: UP/DOWN scroll, **SELECT runs the webhook again** to refresh, BACK closes it.

### Example: Home Assistant

Home Assistant webhook triggers always answer with an empty response, so use the REST API to read values.
Create a long-lived access token (profile → Security), ideally for a separate non-admin user, and add it as a custom header:

- **Read a sensor:** `GET https://<your-ha>/api/states/sensor.living_room_temperature`
  - Header: `Authorization` = `Bearer <token>`
  - Show on watch: *Value from a JSON response*, JSON path `state`, format `{value} °C`
- **Several values at once** (needs an admin token): `POST https://<your-ha>/api/template`
  - Header: `Authorization` = `Bearer <token>`, body: `{"template": "Garage: {{ states('cover.garage') }}\nInside: {{ states('sensor.living_room_temperature') }} °C"}`
  - Show on watch: *Response text*

## Reliability

- The webhook list is cached on the watch, so it shows up instantly and without a phone connection. Webhooks are triggered by their id, so a list that is out of date can never run the wrong webhook; the phone then sends the current list.
- Every trigger ends with feedback on the watch: *Success*, *Error*, *Timeout*, *Status …*, *No phone* (watch not connected), *Not sent* (phone did not accept the message) or *No response* (no answer within 20 seconds). The header shows *Sending...* while a request is running.
- Requests time out after 10 seconds, also on phone apps whose JavaScript engine ignores `XMLHttpRequest.timeout`.
- The settings page result is read correctly on both Android and iOS (URI-encoded or not).
