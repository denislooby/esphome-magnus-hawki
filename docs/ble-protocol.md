# Magnus HAWKi BLE Protocol

Reverse-engineered notes on the Magnus HAWKi BLE interface. No authentication or bonding is required. The device only accepts one BLE connection at a time.

## Device

- **Chip:** Nordic nRF52840 (QIAA)
- **Radar:** Acconeer pulsed coherent radar
- **Power:** CR123A battery
- **Connectivity:** BLE only, no WiFi or cloud on the consumer version
- **Advertised name:** `Magnus-{DEVICE_ID}-{SUFFIX}`
- **Advertised service:** `00001923-1322-ffdd-3533-886fdadce133`

## Services

| Name | UUID | Notes |
|---|---|---|
| Device Information | `180A` | Standard BLE service |
| SMP | `8d53dc1d-1db7-4cd3-868b-8a527460aa84` | Nordic McuMgr firmware update (DFU) |
| Main | `00001923-1322-ffdd-3533-886fdadce133` | Measurement, debug log, tank configuration |
| Data | `00002010-1322-ffdd-3533-886fdadce133` | Measurement result, serial/production info |

The Main and Data characteristic UUIDs follow the pattern `0000XXXX-1322-ffdd-3533-886fdadce133`, where `XXXX` is the short ID used throughout this document.

## Characteristic Map

Status key:
- **Used**: used by the `magnus_hawki` component, behaviour confirmed
- **Known**: purpose confirmed, not used by the component
- **Probable**: purpose guessed from the value, not confirmed
- **Unknown**: purpose not identified

Values are as read from one device in February 2026. Multi-byte values are shown as raw hex.

### Device Information (`180A`)

| Char | Value |
|---|---|
| `2A24` Model Number | `nRF52840_QIAA` |
| `2A29` Manufacturer Name | `Manufacturer` |

### SMP (`8d53dc1d-...`)

| Char | Notes |
|---|---|
| `da2e7828-fbce-4e01-ae9e-261174997c48` | McuMgr SMP characteristic (notify). Firmware update only |

### Main (`00001923-...`)

| Char | Type | Status | Example value | Description |
|---|---|---|---|---|
| `1924` | Read | Known | ASCII 12-char hex string | Device ID (matches the ID in the advertised name) |
| `1930` | Read | Unknown | `0F` (15) | Not battery (battery is in the [advertisement](#advertisement-data)). Read `0F` on two devices |
| `1934` | Write | Unknown | | Write-only. Probably a command/config input |
| `1960` | Notify | Used | `01` → `02` → `05` | Measurement trigger. Subscribing starts a radar measurement. States: `01` starting, `02` measuring, `05` complete |
| `1961` | Indicate | Unknown | | Subscribing delivered no data |
| `1962` | Read | Used | `00095 001 304 0` | Last measurement, ASCII. See [`1962` format](#1962-last-measurement) |
| `1963` | Write | Unknown | | Write-only |
| `1964` | Read | Unknown | `02` | |
| `1965` | Read | Known | ASCII `0106` | Firmware version |
| `1967` | Read | Unknown | `0B` (11) | Not battery. Read `0B`/`0C` on two devices; possibly a temperature |
| `1968` | Read | Unknown | `00000000` | Possibly a counter |
| `1969` | Notify | Used | ASCII | Radar debug log. **Must be subscribed before triggering or the radar does not run.** See [Debug log](#1969-debug-log) |
| `1978` | Read | Unknown | `02` | |
| `1990` | Read | Probable | `08 03 13` | Tank configuration |
| `1991` | Read | Probable | `00C8 0140` (200, 320) | Tank dimensions in mm |
| `1992` | Read | Probable | `01` | Tank type |
| `1993` | Read | Known | `15.02.2026 21:04:54` | Device timestamp of last measurement, ASCII `DD.MM.YYYY HH:MM:SS`. Not used by the component |
| `1994` | Read | Unknown | `0001E240` (123456) | Did not change between readings |
| `1995` | Read | Unknown | `0000000C` (12) | Possibly a measurement count |
| `1996` | Read | Probable | `1900 1F40` | Tank configuration |
| `1997` | Read | Unknown | `00000014` (20) | |
| `1998` | Read | Unknown | `0100` | Possibly config flags |
| `1999` | Write | Unknown | | Write-only |

### Data (`00002010-...`)

| Char | Type | Status | Example value | Description |
|---|---|---|---|---|
| `2011` | Read | Probable | ASCII, format `1-XXXXXXXXXXXXXXXXX` | Serial / part number |
| `2012` | Read | Probable | ASCII, 19-digit string | Production code |
| `2013` | Read | Probable | ASCII, 19-digit string | Hardware revision |
| `2014` | Notify | Used | `distance_mid  95/0` | Fresh measurement result. See [`2014` format](#2014-fresh-result) |
| `2015` | Read | Unknown | `00` | |
| `2020` | Notify | Unknown | | Subscribing delivered no data during a measurement |
| `2021` | Read | Unknown | | Not read yet |

## Value Formats

### `1962` (last measurement)

ASCII, space-separated: `{distance} {field2} {field3} {status}`, e.g. `00095 001 304 0`.

| Field | Example | Meaning |
|---|---|---|
| 1 | `00095` | Distance from sensor to oil surface in mm, zero-padded. `99998`/`99999` = no target |
| 2 | `001` | Unknown. Seen values 000–007 |
| 3 | `304` | Unknown. Seen values 302–311, barely changes. Possibly a reference/calibration value |
| 4 | `0` | Probably a status/error code |

> **Note:** Early nRF Connect sessions only ever saw `99998`/`99999` in field 1, so the first conclusion was that `1962` did not hold the oil distance. Later ESP32 probe runs disproved this. After a fresh measurement returned `distance_mid  92/0` on `2014`, re-reading `1962` gave `00092 001 303 0`. The `9999x` values were "no target" readings.

### `2014` (fresh result)

ASCII: `distance_mid  {mm}/{status}` (two spaces), e.g. `distance_mid  95/0`. The distance is the value between the last space and the `/`.

### `1969` (debug log)

ASCII notifications, each ending in `;;/0`. A measurement runs three subsweeps (close, mid, far), and the best result is selected:

```
Powering on sensor
Sweeps per frame:1
Number of subsweeps:3

[Subsweep 0 - Close] Offset: 0, Length: 160
  Converting 160 complex values into magnitude
  Applying SMA filter with window size 5
  close_meas: 0

[Subsweep 1 - Mid] Offset: 160, Length: 104
  ...
  mid_meas: 1
  Max amplitude index: 34
  Calculated Depth: 155

[Subsweep 2 - Far] Offset: 264, Length: 214
  ...
  far_meas: 1
  Max amplitude index: 0
  Calculated Depth: 300

Final: Measured Radar Depth = 135
```

(Condensed from a real trace.)

### Advertisement data

The HAWKi broadcasts its latest reading in the manufacturer-specific data of its advertisements, so it can be read without connecting. The Magnus app reads these values from the advertisement too: its log prints `Adv data: 3306F2AEF62F70900213` → `Level: 767; Timestamp (UTC): Sat, 07 Nov 2026 12:58:11 GMT; FW ver: 19; Battery: 82`.

The payload is 10 bytes. BLE stacks report the first two bytes as the "company ID", but they are part of the payload and change with every reading. Read the 10 bytes as a single little-endian 80-bit integer:

| Bits | Width | Field | Example (`3306F2AEF62F70900213`) |
|---|---|---|---|
| 0–3 | 4 | Format, always `3` so far | 3 |
| 4–35 | 32 | Reading time, Unix seconds, device clock | 1794056291 (2026-11-07 12:58:11 UTC) |
| 36–49 | 14 | Distance in mm. `16382`/`16383` = no target (the 14-bit counterparts of `99998`/`99999`) | 767 |
| 50–58 | 9 | Unknown. Seen 0, 24, 28, 36 | 28 |
| 59–65 | 7 | Battery %, the value the Magnus app shows | 82 |
| 66–71 | 6 | Unused, always 0 so far | 0 |
| 72–79 | 8 | Firmware version | 19 |

Behaviour seen on one device in November 2026:

- The advertisement normally carries the hourly reading. The Magnus app shows "Measurements are taken hourly".
- After any connection attempt (Magnus app or ESP32, even a failed one), the reading time advances every 5–35 seconds for about 3–4 minutes, then returns to hourly. The distance varied during a burst (767 → 772 → 767 mm), so these are real radar measurements.
- The device clock ran about 7 min 20 s fast, so reading times can be slightly in the future.
- Battery can flip by 1% between consecutive readings (81/82).

The two samples quoted in earlier versions of this document (`23a9da98e6ff63e00213`, `53a7ee98e6ff03d00213`) decode to 2026-02-12 and 2026-02-13, distance `16382` (no target), battery 92% and 90%, firmware 19.

### Special values

- **Distance `99999` / `99998`**: no radar target detected (empty tank, misaligned sensor, or low battery). Treat it as "no reading", not as a real distance.
- **Advertised distance `16383` / `16382`**: the same "no target" condition in the 14-bit advertisement field.

## Reading the Last Measurement

The device measures on its own schedule and stores the result in `1962`. Reading it does not explicitly start the radar, but any connection triggers a few minutes of repeated measurements (see [Advertisement data](#advertisement-data)). Connecting therefore costs battery; the advertisement carries the same reading for free.

1. Connect and run service discovery
2. Read `1962`
3. Parse field 1 as distance in mm
4. Disconnect

## Triggering a Fresh Measurement

Subscription order matters:

1. Subscribe to `1969` (debug log). Without this the radar never starts.
2. Subscribe to `2014` (result)
3. Subscribe to `1960` (trigger). The subscription itself starts the measurement.
4. `1960` notifies `01` → `02` → `05` over about 6 seconds. `1969` streams debug messages meanwhile.
5. `2014` notifies the result, e.g. `distance_mid  95/0`
6. Disconnect

After a fresh measurement, `1962` and `1993` hold the new value and time.

## Distance to Level

The radar measures from the sensor down to the oil surface, so distance increases as the tank empties:

```
oil_depth = tank_height - (distance - offset)
level_%   = oil_depth / tank_height * 100     (clamped to 0–100)
```

`tank_height` is the distance from the sensor to the outflow pipe, and `offset` is the distance from the sensor to the tank top. These are the same values entered in the Magnus app.

## Open Questions

- **Unknown advertisement field** (bits 50–58): seen 0, 24, 28 and 36. Possibly temperature or a counter.
- **Measurement bursts after connecting:** how much battery does a burst cost, and can it be avoided while still reading `1962`?
- **`1930` and `1967`:** not battery. Meaning unknown.
- **Write-only characteristics** (`1934`, `1963`, `1999`): not explored.
- **Tank configuration** (`1990`, `1991`, `1992`, `1996`): the exact mapping to the app's settings is unconfirmed.

## How This Was Discovered

1. **nRF Connect (iOS):** connected to the device, listed all services and characteristics, read every readable value, and subscribed to the notify characteristics. This showed that `1960` triggers a measurement and that `2014` delivers the result.
2. **ESP32 probe component:** a throwaway ESPHome component that resolves every known characteristic, reads them all, subscribes to the notify characteristics, triggers a measurement, then re-reads `1962` and `1993`. This confirmed that `1962` holds the last distance.
3. **Component development:** building the `magnus_hawki` component showed that the radar does not run unless `1969` is subscribed first.
4. **Advertisement decoding:** the Magnus app's log screen prints each advertisement's raw payload next to its decoded level, timestamp, firmware and battery. Matching those values bit by bit gave the layout above. Live decoding on an ESP32 then agreed with the connected `1962` reads.

The raw nRF Connect session logs and the ESP32 probe output are kept privately and are not published in this repository.
