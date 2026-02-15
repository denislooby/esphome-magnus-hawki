#pragma once

#include "esphome/core/component.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/button/button.h"

#ifdef USE_ESP32

namespace esphome {
namespace magnus_hawki {

enum class OpMode {
  IDLE,             // Disconnected, waiting for next update or button press
  CACHE_CONNECT,    // Connecting to read cached values
  CACHE_READ,       // Reading 1962 + 1993
  MEASURE_CONNECT,  // Connecting for fresh measurement
  MEASURE_SUBSCRIBE,// Subscribing to 1969, 2014
  MEASURE_TRIGGER,  // Subscribed to 1960, waiting for result
  MEASURE_DONE,     // Got result, reading timestamp
  DISCONNECTING,    // About to disconnect
};

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
  void set_offset(float offset) { this->offset_ = offset; }

  /// Called by the button to trigger a fresh radar measurement
  void trigger_fresh_measurement();

 protected:
  void parse_distance_(const uint8_t *data, uint16_t length, bool from_cache);
  void parse_timestamp_(const uint8_t *data, uint16_t length);
  void schedule_disconnect_();
  void do_disconnect_();

  sensor::Sensor *distance_sensor_{nullptr};
  sensor::Sensor *level_sensor_{nullptr};
  text_sensor::TextSensor *timestamp_sensor_{nullptr};

  uint16_t trigger_handle_{0};   // 1960
  uint16_t result_handle_{0};    // 2014
  uint16_t timestamp_handle_{0}; // 1993
  uint16_t cached_handle_{0};    // 1962
  uint16_t debug_handle_{0};     // 1969

  float tank_height_{0};
  float offset_{0};
  OpMode mode_{OpMode::IDLE};
  bool fresh_measure_requested_{false};
  uint32_t operation_start_time_{0};
  static const uint32_t OPERATION_TIMEOUT_MS = 30000;

  int cache_reads_pending_{0};  // Track outstanding reads
};

/// Button that triggers a fresh radar measurement
class MagnusHawkiMeasureButton : public button::Button, public Parented<MagnusHawki> {
 public:
  void press_action() override { this->parent_->trigger_fresh_measurement(); }
};

}  // namespace magnus_hawki
}  // namespace esphome

#endif  // USE_ESP32
