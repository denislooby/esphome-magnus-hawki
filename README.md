# ESPHome Magnus HAWKi BLE Component

ESPHome external component for the **Magnus HAWKi** radar oil tank level monitor. Reads oil level via BLE from an ESP32 and exposes it to Home Assistant.

## What is the Magnus HAWKi?

The [Magnus HAWKi](https://magnusmonitors.com/) is a battery-powered radar sensor that measures oil tank levels through plastic without drilling. It uses an Acconeer pulsed coherent radar on a Nordic nRF52840 chip, communicating via BLE only (no WiFi/cloud on consumer version).

## Hardware Requirements

- **ESP32** board (any variant with BLE support)
- ESP32 must be within BLE range of the HAWKi sensor (~10m through walls)
- The HAWKi sensor installed on your oil tank

## Installation

Add to your ESPHome YAML:

```yaml
external_components:
  - source: github://denislooby/esphome-magnus-hawki
    components: [magnus_hawki]
```

## Configuration

### Full Example

```yaml
esp32_ble_tracker:

ble_client:
  - mac_address: "XX:XX:XX:XX:XX:XX"
    id: hawki_ble

magnus_hawki:
  ble_client_id: hawki_ble
  tank_height: 1040      # mm - distance from sensor to outflow pipe
  offset: 0              # mm - distance from sensor to tank top (0 if flush)
  update_interval: 14400s

sensor:
  - platform: magnus_hawki
    distance:
      name: "Oil Tank Distance"
    level:
      name: "Oil Tank Level"

text_sensor:
  - platform: magnus_hawki
    timestamp:
      name: "Oil Tank Last Reading"

button:
  - platform: magnus_hawki
    measure:
      name: "Oil Tank Measure Now"
```

### Configuration Variables

**Hub (`magnus_hawki`)**

| Variable | Required | Default | Description |
|---|---|---|---|
| `ble_client_id` | Yes | - | ID of the `ble_client` connected to your HAWKi |
| `tank_height` | No | - | Distance from sensor to outflow pipe in mm (usable oil depth) |
| `offset` | No | `0` | Distance from sensor to tank top in mm (0 if flush-mounted) |
| `update_interval` | No | `14400s` | How often to read cached data (4 hours) |

**Sensor Platform**

| Sensor | Unit | Description |
|---|---|---|
| `distance` | mm | Raw distance from radar sensor to oil surface |
| `level` | % | Oil level percentage (requires `tank_height`) |

**Text Sensor Platform**

| Sensor | Description |
|---|---|
| `timestamp` | Last measurement time from the device |

## Finding Your Device MAC Address

The Magnus HAWKi advertises as `Magnus-{ID}-{suffix}`. To find the MAC address:

1. Use the ESPHome BLE tracker in scan mode:
   ```yaml
   esp32_ble_tracker:
     scan_parameters:
       active: true
   ```
2. Check the ESPHome logs for a device starting with `Magnus-`
3. Or use nRF Connect app on your phone to scan for nearby BLE devices

## Tank Calibration

These match the values you set during Magnus app setup:

- **`offset`**: Distance from the sensor to the top of the tank opening (in mm). Set to `0` if the sensor is flush-mounted at the tank top.
- **`tank_height`**: Distance from the sensor down to the outflow pipe (in mm). This is the usable oil depth — the distance the oil can fill from outflow pipe up to the sensor.

The level percentage is calculated as: `level = ((tank_height - (distance - offset)) / tank_height) * 100`

For example, with `offset: 0` and `tank_height: 1040`, a distance reading of 95mm gives: `(1040 - 95) / 1040 = 90.9%`

## How It Works

The component operates in two modes to preserve the HAWKi's CR123A battery:

**Normal mode (periodic cache read):**
1. **Connect** to the HAWKi via `ble_client`
2. **Read** characteristic `1962` for the cached measurement
3. **Read** characteristic `1993` for the timestamp
4. **Disconnect** immediately (~3 seconds total)

**Fresh measurement (via button press in HA):**
1. **Connect** and subscribe to `1969` (debug log — required for radar to power on)
2. **Subscribe** to `2014` (result) then `1960` (trigger)
3. The device runs through states `01` -> `02` -> `05` (~6 seconds)
4. **Receive** the result on `2014` as ASCII: `distance_mid  135/0`
5. **Read** timestamp from `1993`, then **disconnect**

The component disconnects after each operation so the Magnus app can connect normally. No authentication or bonding is required.

## Troubleshooting

**Device not connecting:**
- Verify the MAC address is correct
- Ensure the ESP32 is within BLE range (check RSSI > -80 dBm)
- The HAWKi only allows one BLE connection at a time - close the Magnus app first

**No distance readings:**
- Check ESPHome logs for "Trigger characteristic (1960) not found" or similar
- The device may need a fresh battery (CR123A)
- Try power-cycling the HAWKi (remove and reinsert battery)

**Level shows 0% or 100% incorrectly:**
- Verify `tank_height` matches your actual tank configuration
- Distance increases as oil level drops (sensor is at the top)

**Stale readings:**
- The default `update_interval` is 4 hours to preserve battery life (CR123A)
- Reduce it for more frequent readings, but this will drain the battery faster
- Each measurement cycle takes ~5-6 seconds of BLE activity

**Distance shows 99999:**
- This means the radar detected no target (empty tank or sensor not aimed correctly)
- The component will log a warning and skip publishing to avoid bogus HA values
- Check sensor alignment and battery level

## BLE Protocol Reference

| Characteristic | Service | Type | Description |
|---|---|---|---|
| `1960` | `00001923-...` | Notify | Measurement trigger (01->02->05) |
| `2014` | `00002010-...` | Notify | Result: `distance_mid  {mm}/{status}` |
| `1993` | `00001923-...` | Read | Timestamp: `DD.MM.YYYY HH:MM:SS` |
| `1962` | `00001923-...` | Read | Cached measurement: `{dist} {n} {ref} {status}` |
| `1969` | `00001923-...` | Notify | Debug log stream (must subscribe for radar to run) |
| `1991` | `00001923-...` | Read | Tank dimensions (height in mm) |

For the complete BLE characteristic map, see [magnus-hawki-ble.md](https://github.com/denislooby/esphome-magnus-hawki/blob/main/docs/ble-protocol.md).

## License

MIT License - see [LICENSE](LICENSE).

## Credits

BLE protocol reverse-engineered using nRF Connect on iOS. No authentication is required to communicate with the device.
