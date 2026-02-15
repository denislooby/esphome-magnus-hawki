#pragma once

#include "esphome/core/component.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"

#ifdef USE_ESP32

namespace espbt = esphome::esp32_ble_tracker;

namespace esphome {
namespace magnus_hawki {

class MagnusHawki : public PollingComponent, public ble_client::BLEClientNode {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;

  void set_distance_sensor(sensor::Sensor *sensor) { this->distance_sensor_ = sensor; }
  void set_level_sensor(sensor::Sensor *sensor) { this->level_sensor_ = sensor; }
  void set_timestamp_sensor(text_sensor::TextSensor *sensor) { this->timestamp_sensor_ = sensor; }
  void set_tank_height(float height) { this->tank_height_ = height; }

 protected:
  void parse_distance_(const uint8_t *data, uint16_t length);
  void parse_timestamp_(const uint8_t *data, uint16_t length);
  void request_timestamp_read_();

  sensor::Sensor *distance_sensor_{nullptr};
  sensor::Sensor *level_sensor_{nullptr};
  text_sensor::TextSensor *timestamp_sensor_{nullptr};

  uint16_t trigger_handle_{0};
  uint16_t result_handle_{0};
  uint16_t timestamp_handle_{0};

  float tank_height_{0};
  bool measurement_pending_{false};
};

}  // namespace magnus_hawki
}  // namespace esphome

#endif  // USE_ESP32
