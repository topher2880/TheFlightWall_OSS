# TheFlightWall

TheFlightWall is an ESP32-powered LED wall that shows live information about aircraft flying near you.

This repository contains the open-source firmware and build information for a 20-panel (10 x 2) WS2812B matrix display. The firmware combines nearby ADS-B state vectors from OpenSky with FlightAware AeroAPI enrichment for flight number, route, operator and aircraft information.

![Main Image](images/main-image.png)

## What it shows

For each nearby aircraft, the wall tries to show three useful lines:

1. **Flight identifier + operator** — for example `SQ212 Singapore Airlines`, `FD212 RFDS`, or `RAAF ASY123`
2. **Route** — friendly airport city names when AeroAPI supplies them, for example `SYDNEY>SINGAPORE`, with IATA/ICAO codes as fallbacks
3. **Aircraft type** — using a friendly aircraft name when available

When a matching airline logo is available, a 32x32 RGB565 logo occupies the left-most panel and the three text lines use the remaining 128 pixels. Flights without a logo (including military and most GA traffic) continue to use the full display width.

The display uses the available width before truncating long text.

### ADS-B fallback

OpenSky is the source of truth for whether an aircraft is physically nearby.

If FlightAware cannot enrich a detected aircraft, the target is **not discarded**. The wall keeps the original ADS-B callsign and shows an ADS-B fallback card. This is useful for GA aircraft, unusual callsigns, and flights where metadata is unavailable.

### RAAF / military fallback

Some military flights expose limited public metadata even though their ADS-B position is visible. The firmware recognises a curated set of Australian military operator and tactical callsigns and displays them explicitly as **RAAF** when a high-confidence match is available.

Current recognised examples include `ASY` (AUSSIE), `BLKT` (BLACKCAT / P-8A), `DRGN` (DRAGON / KC-30A), `WNSR` (WINDSOR / KC-30A), `DNGO` (DINGO / King Air), `EVY` (ENVOY), `DGTL`, `WGTL`, `OBAK`, `STAL`, and `WLBY`.

The matcher deliberately avoids very broad prefixes such as `BLK`, because similar tactical callsigns can be used by other military operators.

For a sparse RAAF record the wall can show something like:

```
RAAF ASY123
YPAD>YAMB
C-17A
```

If route or aircraft metadata is unavailable, useful labels such as `MILITARY FLIGHT` are shown instead of leaving the card effectively anonymous. Airport labels prefer AeroAPI's city field (for example `ADELAIDE`, `DENPASAR`, or `HONG KONG`), then airport name, IATA code, and finally ICAO code. When only one end of the route is known, the wall shows `FROM <airport>` or `TO <airport>` rather than a dangling route arrow. For several well-known tactical callsigns, a conservative aircraft hint is also available as a fallback.

The fallback design is intentionally independent of FlightRadar24 filtering; OpenSky supplies the nearby ADS-B target and FlightAware remains the primary enrichment source.

## Component List

### Main components

- 20x [16x16 LED panels](https://www.aliexpress.us/item/2255800358269772.html)
- ESP32 development board (the original build used an [R32 D1](https://www.amazon.com/HiLetgo-ESP-32-Development-Bluetooth-Arduino/dp/B07WFZCBH8), but a suitable ESP32 board should work)
- 3D printed brackets, MDF, or another backing/mounting system
- Two horizontal support pieces

### Power

- [5V >20A power supply](https://www.amazon.com/dp/B07KC55TJF) for a 20-panel build
- [3.3V to 5V level shifter](https://www.amazon.com/dp/B07F7W91LC)

### Data

- [OpenSky](https://opensky-network.org/) for nearby ADS-B state vectors and callsigns
- [FlightAware AeroAPI](https://www.flightaware.com/commercial/aeroapi/) for enrichment such as route, operator, flight identifier and aircraft
- FlightWall CDN lookups for friendly airline and aircraft names

## Hardware

### Dimensions

With 20 panels arranged 10 x 2, the matrix is 160 x 32 pixels and approximately 63 inches x 12.6 inches with the original panels.

### LED Panels

[These are the LED panels used by the original project](https://www.aliexpress.us/item/2255800358269772.html), but similar WS2812B matrix tiles can be adapted.

3D printable brackets can attach the panels together, or the tiles can be mounted to MDF or another suitable backing.

![LED Panel Wiring and Brackets](images/led-panel-wiring-and-brackets.jpg)

### Wiring

![Wiring Diagram](images/wiring-diagram.png)

The entire panel is controlled by one data line. That keeps the electronics simple, although it is not intended to be a high-frame-rate display.

## Data and Software

### Data flow

The firmware follows this sequence:

1. Query OpenSky for aircraft within the configured radius.
2. Preserve every usable ADS-B callsign as a displayable target.
3. Query FlightAware AeroAPI for richer flight metadata.
4. Look up friendly airline and aircraft names when identifiers are available.
5. Render either the enriched flight card or a useful fallback card.

This means a metadata/API miss no longer causes an otherwise valid nearby aircraft to disappear from the wall.

## API Keys

### OpenSky

1. Register for an [OpenSky](https://opensky-network.org/) account.
2. Open your OpenSky account page.
3. Create an API client.
4. Add the `client_id` and `client_secret` to [APIConfiguration.h](firmware/config/APIConfiguration.h).

### FlightAware AeroAPI

1. Create a FlightAware AeroAPI account.
2. Create an API key from the AeroAPI dashboard.
3. Add the key to [APIConfiguration.h](firmware/config/APIConfiguration.h).

Do not commit real API keys or Wi-Fi credentials to a public repository.

## Configuration

### Wi-Fi

Enter your Wi-Fi credentials into `WIFI_SSID` and `WIFI_PASSWORD` in [WiFiConfiguration.h](firmware/config/WiFiConfiguration.h).

### Location

Set the centre point and tracking radius in [UserConfiguration.h](firmware/config/UserConfiguration.h):

- `CENTER_LAT`
- `CENTER_LON`
- `RADIUS_KM`

### Display

Display hardware settings are in [HardwareConfiguration.h](firmware/config/HardwareConfiguration.h).

The default layout is:

- 16 x 16 pixels per tile
- 10 tiles horizontally
- 2 tiles vertically
- 160 x 32 pixels total

Brightness and text colour can be changed in [UserConfiguration.h](firmware/config/UserConfiguration.h).

## Build and Flash with PlatformIO

1. Install [VS Code](https://code.visualstudio.com/).
2. Install the [PlatformIO IDE extension](https://platformio.org/install/ide?install=vscode).
3. Open the `firmware` folder in PlatformIO.
4. Configure:
   - API credentials in `config/APIConfiguration.h`
   - Wi-Fi in `config/WiFiConfiguration.h`
   - location/display preferences in `config/UserConfiguration.h`
   - matrix hardware in `config/HardwareConfiguration.h`
5. Connect the ESP32 over USB.
6. Upload the LittleFS logo image with `pio run -d firmware -t uploadfs` (or PlatformIO's **Upload Filesystem Image** task).
7. Build and upload the firmware with `pio run -d firmware -t upload` (or the normal PlatformIO **Upload** button).

The logo filesystem only needs to be re-uploaded when logo assets change; ordinary firmware-only changes can use the normal upload step.

## Display behaviour notes

Flight identifiers are deliberately placed at the **front** of the first line so a long operator name cannot truncate away the most useful identifier.

Identifier preference is:

1. IATA identifier when available
2. FlightAware primary ident
3. ICAO identifier
4. Original OpenSky ADS-B callsign

That makes commercial flight numbers more readable while still preserving unusual, GA, RFDS and military callsigns.

### Airline logo assets

The initial logo library is adapted from the public `biohead/TheFlightWall_OSS` and `LuckierTrout/TheFlightWall_OSS-main` forks. Their FlightWall code is published under Apache-2.0. Airline names and logos remain trademarks of their respective owners.

The current bundled set prioritises airlines likely to appear around Australia and common international overflight routes, including Qantas, Jetstar, Virgin Australia, Air New Zealand, Fiji Airways, Singapore Airlines, Qatar Airways, Emirates, Cathay Pacific, Vietnam Airlines, Scoot, Malaysia Airlines, Thai Airways, Philippine Airlines, Batik Air, JAL, ANA, Korean Air and major Chinese carriers.

Missing logos are non-fatal: the wall simply renders the normal full-width text card.

## Original project

The original FlightWall project and commercial displays can be found at [theflightwall.com](https://theflightwall.com).

This repository is intended for people building and modifying their own FlightWall-style display.
