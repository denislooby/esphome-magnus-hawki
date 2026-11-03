# Magnus HAWKi BLE Protocol

Reverse-engineered notes on the Magnus HAWKi BLE interface. No authentication or bonding is required. The device only accepts one BLE connection at a time.

## Services

| Name | UUID |
|---|---|
| Main | `00001923-1322-ffdd-3533-886fdadce133` |
| Data | `00002010-1322-ffdd-3533-886fdadce133` |

All characteristic UUIDs follow the same pattern: `0000XXXX-1322-ffdd-3533-886fdadce133`, where `XXXX` is the short ID used throughout this document.

## Characteristic Map

Status key:
- **Used**: used by the `magnus_hawki` component, behaviour confirmed
- **Known**: purpose confirmed, not used by the component
- **Probable**: purpose guessed from the value read, not confirmed
- **Unknown**: purpose not identified

### Main service (`00001923-...`)

| Char | Type | Status | Description |
|---|---|---|---|
| `1924` | Read | Probable | Device ID |
| `1930` | Read | Unknown | Battery candidate (raw value logged during cache reads) |
| `1960` | Notify | Used | Measurement trigger. Subscribing starts a radar measurement. Notifies state bytes `01` (starting), `02` (measuring), `05` (complete) |
| `1961` | Notify | Unknown | Indicate |
| `1962` | Read | Used | Cached (last) measurement, ASCII: `{dist} {n} {ref} {status}`, e.g. `00095 001 304 0`. First field is distance in mm, zero-padded |
| `1964` | Read | Unknown | |
| `1965` | Read | Probable | Firmware version |
| `1967` | Read | Unknown | Battery candidate (raw value logged during cache reads) |
| `1968` | Read | Unknown | |
| `1969` | Notify | Used | Debug log stream, ASCII, messages end in `;;/0`. **Must be subscribed before triggering or the radar does not run** |
| `1978` | Read | Unknown | |
| `1990` | Read | Probable | Tank type |
| `1991` | Read | Known | Tank dimensions (height in mm) |
| `1992` | Read | Probable | Tank shape |
| `1993` | Read | Known | Device timestamp of last measurement, ASCII: `DD.MM.YYYY HH:MM:SS` |
| `1994` | Read | Probable | Measurement interval |
| `1995`–`1998` | Read | Unknown | |

### Data service (`00002010-...`)

| Char | Type | Status | Description |
|---|---|---|---|
| `2011` | Read | Probable | Radar config 1 |
| `2012` | Read | Probable | Radar config 2 |
| `2013` | Read | Probable | Radar config 3 |
| `2014` | Notify | Used | Fresh measurement result, ASCII: `distance_mid  {mm}/{status}`, e.g. `distance_mid  95/0` |
| `2015` | Read | Unknown | |
| `2020` | Notify | Unknown | |
| `2021` | Read | Unknown | |

## Reading the Cached Measurement

The device stores its last measurement in `1962`. Reading it does not power the radar, so it is cheap on the battery.

1. Connect and run service discovery
2. Read `1962`
3. Parse the first space-separated field as distance in mm
4. Disconnect

## Triggering a Fresh Measurement

Subscription order matters:

1. Subscribe to `1969` (debug log). Without this the radar never starts.
2. Subscribe to `2014` (result)
3. Subscribe to `1960` (trigger). The subscription itself starts the measurement.
4. `1960` notifies `01` → `02` → `05` over about 6 seconds. `1969` streams debug messages meanwhile.
5. `2014` notifies the result, e.g. `distance_mid  95/0`. The distance is the value between the last space and the `/`.
6. Disconnect

After a fresh measurement, `1962` and `1993` hold the new value.

## Special Values

- **Distance `99999`**: no radar target detected (empty tank, misaligned sensor, or low battery). Treat it as "no reading", not as a real distance.

## Distance to Level

The radar measures from the sensor down to the oil surface, so distance increases as the tank empties:

```
oil_depth = tank_height - (distance - offset)
level_%   = oil_depth / tank_height * 100     (clamped to 0–100)
```

`tank_height` is the distance from the sensor to the outflow pipe, and `offset` is the distance from the sensor to the tank top. These are the same values entered in the Magnus app.
