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
  tank_height: 320
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
```

### Configuration Variables

**Hub (`magnus_hawki`)**

| Variable | Required | Default | Description |
|---|---|---|---|
| `ble_client_id` | Yes | - | ID of the `ble_client` connected to your HAWKi |
| `tank_height` | No | - | Tank height in mm (needed for level % calculation) |
| `update_interval` | No | `14400s` | How often to trigger a new measurement (4 hours) |

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

## Tank Height Configuration

The `tank_height` value is the total distance (in mm) from the radar sensor to the bottom of the tank when empty. You can find this from:

- **Characteristic `1991`** on the device (read via nRF Connect): contains two values, the second being tank height in mm
- **The Magnus app**: check your tank configuration settings
- **Physical measurement**: measure from sensor mounting point to tank bottom

The level percentage is calculated as: `level = ((tank_height - distance) / tank_height) * 100`

## How It Works

The component uses the HAWKi's BLE notification protocol:

1. **Connect** to the HAWKi via `ble_client`
2. **Subscribe** to characteristic `2014` (result notifications)
3. **Subscribe** to characteristic `1960` (triggers the radar measurement)
4. The device runs through states `01` -> `02` -> `05` (measuring)
5. **Receive** the result on `2014` as ASCII: `distance_mid  135/0`
6. **Parse** the distance value and calculate level percentage
7. **Read** characteristic `1993` for the measurement timestamp

No authentication or bonding is required.

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
| `1962` | `00001923-...` | Read | Raw radar reference data (not oil level) |
| `1991` | `00001923-...` | Read | Tank dimensions (height in mm) |

For the complete BLE characteristic map, see [magnus-hawki-ble.md](https://github.com/denislooby/esphome-magnus-hawki/blob/main/docs/ble-protocol.md).

## License

MIT License - see [LICENSE](LICENSE).

## Credits

BLE protocol reverse-engineered using nRF Connect on iOS. No authentication is required to communicate with the device.
