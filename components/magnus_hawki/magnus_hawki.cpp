#include "magnus_hawki.h"

#ifdef USE_ESP32

#include "esphome/core/log.h"
#include <cstdlib>
#include <cstring>
#include <string>

namespace esphome {
namespace magnus_hawki {

static const char *const TAG = "magnus_hawki";

// Service UUIDs
static const esp32_ble_tracker::ESPBTUUID SERVICE_MAIN =
    esp32_ble_tracker::ESPBTUUID::from_raw("00001923-1322-ffdd-3533-886fdadce133");
static const esp32_ble_tracker::ESPBTUUID SERVICE_DATA =
    esp32_ble_tracker::ESPBTUUID::from_raw("00002010-1322-ffdd-3533-886fdadce133");

// Characteristic UUIDs (short form within their service)
static const esp32_ble_tracker::ESPBTUUID CHAR_TRIGGER =
    esp32_ble_tracker::ESPBTUUID::from_raw("00001960-1322-ffdd-3533-886fdadce133");
static const esp32_ble_tracker::ESPBTUUID CHAR_RESULT =
    esp32_ble_tracker::ESPBTUUID::from_raw("00002014-1322-ffdd-3533-886fdadce133");
static const esp32_ble_tracker::ESPBTUUID CHAR_TIMESTAMP =
    esp32_ble_tracker::ESPBTUUID::from_raw("00001993-1322-ffdd-3533-886fdadce133");

void MagnusHawki::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Magnus HAWKi...");
}

void MagnusHawki::dump_config() {
  ESP_LOGCONFIG(TAG, "Magnus HAWKi:");
  ESP_LOGCONFIG(TAG, "  Tank Height: %.0f mm", this->tank_height_);
  LOG_SENSOR("  ", "Distance", this->distance_sensor_);
  LOG_SENSOR("  ", "Level", this->level_sensor_);
  LOG_TEXT_SENSOR("  ", "Timestamp", this->timestamp_sensor_);
  LOG_UPDATE_INTERVAL(this);
}

void MagnusHawki::update() {
  if (this->node_state != esp32_ble_tracker::ClientState::ESTABLISHED) {
    ESP_LOGD(TAG, "Not connected, skipping update");
    return;
  }

  if (this->measurement_pending_) {
    ESP_LOGD(TAG, "Measurement already pending, skipping");
    return;
  }

  // The HAWKi triggers a measurement when a client subscribes to 1960.
  // To trigger a new measurement, unsubscribe then re-subscribe.
  if (this->trigger_handle_ != 0) {
    ESP_LOGD(TAG, "Triggering new measurement (unsubscribe/resubscribe 1960)...");
    esp_ble_gattc_unregister_for_notify(
        this->parent()->get_gattc_if(), this->parent()->get_remote_bda(),
        this->trigger_handle_);
    // The re-subscribe happens after unregister completes (in UNREG_FOR_NOTIFY_EVT)
    // but we can also just re-register immediately — the ESP stack handles ordering.
    this->measurement_pending_ = true;
    auto status = esp_ble_gattc_register_for_notify(
        this->parent()->get_gattc_if(), this->parent()->get_remote_bda(),
        this->trigger_handle_);
    if (status != ESP_OK) {
      ESP_LOGW(TAG, "Failed to re-register trigger notify: %d", status);
      this->measurement_pending_ = false;
    }
  }
}

void MagnusHawki::gattc_event_handler(esp_gattc_cb_event_t event,
                                       esp_gatt_if_t gattc_if,
                                       esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_OPEN_EVT: {
      if (param->open.status == ESP_GATT_OK) {
        ESP_LOGI(TAG, "Connected to Magnus HAWKi");
      } else {
        ESP_LOGW(TAG, "Connection failed, status=%d", param->open.status);
      }
      break;
    }

    case ESP_GATTC_DISCONNECT_EVT: {
      ESP_LOGW(TAG, "Disconnected from Magnus HAWKi");
      this->trigger_handle_ = 0;
      this->result_handle_ = 0;
      this->timestamp_handle_ = 0;
      this->measurement_pending_ = false;
      break;
    }

    case ESP_GATTC_SEARCH_CMPL_EVT: {
      // Service discovery complete - find our characteristics
      auto *trigger_chr = this->parent()->get_characteristic(SERVICE_MAIN, CHAR_TRIGGER);
      if (trigger_chr == nullptr) {
        ESP_LOGW(TAG, "Trigger characteristic (1960) not found");
      } else {
        this->trigger_handle_ = trigger_chr->handle;
        ESP_LOGD(TAG, "Found trigger handle: 0x%x", this->trigger_handle_);
      }

      auto *result_chr = this->parent()->get_characteristic(SERVICE_DATA, CHAR_RESULT);
      if (result_chr == nullptr) {
        ESP_LOGW(TAG, "Result characteristic (2014) not found");
      } else {
        this->result_handle_ = result_chr->handle;
        ESP_LOGD(TAG, "Found result handle: 0x%x", this->result_handle_);
      }

      auto *timestamp_chr = this->parent()->get_characteristic(SERVICE_MAIN, CHAR_TIMESTAMP);
      if (timestamp_chr == nullptr) {
        ESP_LOGW(TAG, "Timestamp characteristic (1993) not found");
      } else {
        this->timestamp_handle_ = timestamp_chr->handle;
        ESP_LOGD(TAG, "Found timestamp handle: 0x%x", this->timestamp_handle_);
      }

      // Register for notifications on result characteristic (2014) first
      if (this->result_handle_ != 0) {
        auto status = esp_ble_gattc_register_for_notify(
            this->parent()->get_gattc_if(), this->parent()->get_remote_bda(),
            this->result_handle_);
        if (status != ESP_OK) {
          ESP_LOGW(TAG, "Failed to register result notify: %d", status);
        }
      }
      break;
    }

    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      if (param->reg_for_notify.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Notify registration failed for handle 0x%x, status=%d",
                 param->reg_for_notify.handle, param->reg_for_notify.status);
        break;
      }

      if (param->reg_for_notify.handle == this->result_handle_) {
        ESP_LOGI(TAG, "Registered for distance result notifications (2014)");
        // Now register for trigger notifications (1960) to start measurement
        if (this->trigger_handle_ != 0) {
          this->measurement_pending_ = true;
          auto status = esp_ble_gattc_register_for_notify(
              this->parent()->get_gattc_if(), this->parent()->get_remote_bda(),
              this->trigger_handle_);
          if (status != ESP_OK) {
            ESP_LOGW(TAG, "Failed to register trigger notify: %d", status);
            this->measurement_pending_ = false;
          }
        }
      } else if (param->reg_for_notify.handle == this->trigger_handle_) {
        ESP_LOGI(TAG, "Registered for trigger notifications (1960) - measurement started");
        this->node_state = esp32_ble_tracker::ClientState::ESTABLISHED;
      }
      break;
    }

    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.handle == this->trigger_handle_) {
        // Measurement state machine: 01 -> 02 -> 05
        if (param->notify.value_len > 0) {
          uint8_t state = param->notify.value[0];
          ESP_LOGD(TAG, "Trigger state: 0x%02x", state);
          if (state == 0x05) {
            ESP_LOGD(TAG, "Measurement complete");
          }
        }
      } else if (param->notify.handle == this->result_handle_) {
        ESP_LOGD(TAG, "Distance result received (%d bytes)", param->notify.value_len);
        this->parse_distance_(param->notify.value, param->notify.value_len);
        this->measurement_pending_ = false;

        // Read timestamp after getting distance
        this->request_timestamp_read_();
      }
      break;
    }

    case ESP_GATTC_READ_CHAR_EVT: {
      if (param->read.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Read failed for handle 0x%x, status=%d",
                 param->read.handle, param->read.status);
        break;
      }

      if (param->read.handle == this->timestamp_handle_) {
        this->parse_timestamp_(param->read.value, param->read.value_len);
      }
      break;
    }

    default:
      break;
  }
}

void MagnusHawki::parse_distance_(const uint8_t *data, uint16_t length) {
  // Data arrives as ASCII: "distance_mid  135/0"
  std::string raw(reinterpret_cast<const char *>(data), length);
  ESP_LOGD(TAG, "Raw distance data: %s", raw.c_str());

  // Find the numeric distance value between last space and '/'
  size_t last_space = raw.rfind(' ');
  if (last_space == std::string::npos) {
    ESP_LOGW(TAG, "Could not parse distance: no space found");
    return;
  }

  size_t slash = raw.find('/', last_space);
  if (slash == std::string::npos) {
    ESP_LOGW(TAG, "Could not parse distance: no slash found");
    return;
  }

  std::string distance_str = raw.substr(last_space + 1, slash - last_space - 1);
  char *end;
  float distance = std::strtof(distance_str.c_str(), &end);
  if (end == distance_str.c_str()) {
    ESP_LOGW(TAG, "Could not parse distance value: '%s'", distance_str.c_str());
    return;
  }

  ESP_LOGI(TAG, "Oil distance: %.0f mm", distance);

  if (this->distance_sensor_ != nullptr) {
    this->distance_sensor_->publish_state(distance);
  }

  if (this->level_sensor_ != nullptr && this->tank_height_ > 0) {
    float level = ((this->tank_height_ - distance) / this->tank_height_) * 100.0f;
    // Clamp to 0-100%
    if (level < 0.0f) level = 0.0f;
    if (level > 100.0f) level = 100.0f;
    ESP_LOGI(TAG, "Oil level: %.1f%%", level);
    this->level_sensor_->publish_state(level);
  }
}

void MagnusHawki::parse_timestamp_(const uint8_t *data, uint16_t length) {
  // Data arrives as ASCII: "13.02.2026 09:56:33"
  std::string timestamp(reinterpret_cast<const char *>(data), length);
  ESP_LOGD(TAG, "Timestamp: %s", timestamp.c_str());

  if (this->timestamp_sensor_ != nullptr) {
    this->timestamp_sensor_->publish_state(timestamp);
  }
}

void MagnusHawki::request_timestamp_read_() {
  if (this->timestamp_handle_ == 0 || this->timestamp_sensor_ == nullptr) {
    return;
  }

  auto status = esp_ble_gattc_read_char(
      this->parent()->get_gattc_if(), this->parent()->get_conn_id(),
      this->timestamp_handle_, ESP_GATT_AUTH_REQ_NONE);
  if (status != ESP_OK) {
    ESP_LOGW(TAG, "Failed to read timestamp: %d", status);
  }
}

}  // namespace magnus_hawki
}  // namespace esphome

#endif  // USE_ESP32
