#include "magnus_hawki.h"

#ifdef USE_ESP32

#include "esphome/core/log.h"
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>

namespace esphome {
namespace magnus_hawki {

static const char *const TAG = "magnus_hawki";

// Service UUIDs
static const esp32_ble_tracker::ESPBTUUID SERVICE_MAIN =
    esp32_ble_tracker::ESPBTUUID::from_raw("00001923-1322-ffdd-3533-886fdadce133");
static const esp32_ble_tracker::ESPBTUUID SERVICE_DATA =
    esp32_ble_tracker::ESPBTUUID::from_raw("00002010-1322-ffdd-3533-886fdadce133");

// Characteristic UUIDs
static const esp32_ble_tracker::ESPBTUUID CHAR_TRIGGER =
    esp32_ble_tracker::ESPBTUUID::from_raw("00001960-1322-ffdd-3533-886fdadce133");
static const esp32_ble_tracker::ESPBTUUID CHAR_RESULT =
    esp32_ble_tracker::ESPBTUUID::from_raw("00002014-1322-ffdd-3533-886fdadce133");
static const esp32_ble_tracker::ESPBTUUID CHAR_CACHED =
    esp32_ble_tracker::ESPBTUUID::from_raw("00001962-1322-ffdd-3533-886fdadce133");
static const esp32_ble_tracker::ESPBTUUID CHAR_DEBUG =
    esp32_ble_tracker::ESPBTUUID::from_raw("00001969-1322-ffdd-3533-886fdadce133");
static const esp32_ble_tracker::ESPBTUUID CHAR_BATT_A =
    esp32_ble_tracker::ESPBTUUID::from_raw("00001930-1322-ffdd-3533-886fdadce133");
static const esp32_ble_tracker::ESPBTUUID CHAR_BATT_B =
    esp32_ble_tracker::ESPBTUUID::from_raw("00001967-1322-ffdd-3533-886fdadce133");

void MagnusHawki::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Magnus HAWKi...");
  // Start with BLE client disabled - we connect on demand
  this->parent()->set_enabled(false);
}

void MagnusHawki::dump_config() {
  ESP_LOGCONFIG(TAG, "Magnus HAWKi:");
  ESP_LOGCONFIG(TAG, "  Tank Height: %.0f mm", this->tank_height_);
  ESP_LOGCONFIG(TAG, "  Sensor Offset: %.0f mm", this->offset_);
  ESP_LOGCONFIG(TAG, "  Mode: cache read (button for fresh measurement)");
  LOG_SENSOR("  ", "Distance", this->distance_sensor_);
  LOG_SENSOR("  ", "Level", this->level_sensor_);
  LOG_TEXT_SENSOR("  ", "Last Read", this->last_read_sensor_);
  LOG_UPDATE_INTERVAL(this);
}

void MagnusHawki::update() {
  if (this->mode_ != OpMode::IDLE) {
    ESP_LOGD(TAG, "Operation in progress, skipping update");
    return;
  }

  // Check if a fresh measurement was requested via button
  if (this->fresh_measure_requested_) {
    ESP_LOGI(TAG, "Fresh measurement requested, connecting...");
    this->fresh_measure_requested_ = false;
    this->mode_ = OpMode::MEASURE_CONNECT;
    this->arm_watchdog_();
    this->parent()->set_enabled(true);
    return;
  }

  // Normal periodic update: connect and read cached values
  ESP_LOGI(TAG, "Periodic update: connecting to read cached values...");
  this->mode_ = OpMode::CACHE_CONNECT;
  this->arm_watchdog_();
  this->parent()->set_enabled(true);
}

void MagnusHawki::trigger_fresh_measurement() {
  if (this->mode_ != OpMode::IDLE) {
    ESP_LOGW(TAG, "Cannot trigger measurement - operation already in progress");
    return;
  }
  ESP_LOGI(TAG, "Fresh radar measurement requested via button");
  this->fresh_measure_requested_ = true;
  // Force an immediate update cycle
  this->update();
}

void MagnusHawki::trigger_cache_read() {
  if (this->mode_ != OpMode::IDLE) {
    ESP_LOGW(TAG, "Cannot read cache - operation already in progress");
    return;
  }
  ESP_LOGI(TAG, "Cache read requested via button");
  this->mode_ = OpMode::CACHE_CONNECT;
  this->arm_watchdog_();
  this->parent()->set_enabled(true);
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
        this->mode_ = OpMode::IDLE;
        this->parent()->set_enabled(false);

        // Retry on connection failure
        if (this->retry_count_ < MAX_RETRIES) {
          this->retry_count_++;
          ESP_LOGI(TAG, "Scheduling retry %d/%d in 30s",
                   this->retry_count_, MAX_RETRIES);
          this->set_timeout("retry", RETRY_DELAY_MS, [this]() { this->update(); });
        } else {
          ESP_LOGW(TAG, "Max retries reached, waiting for next update cycle");
          this->retry_count_ = 0;
        }
      }
      break;
    }

    case ESP_GATTC_DISCONNECT_EVT: {
      ESP_LOGD(TAG, "Disconnected from Magnus HAWKi");
      this->trigger_handle_ = 0;
      this->result_handle_ = 0;
      this->cached_handle_ = 0;
      this->debug_handle_ = 0;
      this->batt_a_handle_ = 0;
      this->batt_b_handle_ = 0;
      if (this->mode_ != OpMode::IDLE) {
        this->mode_ = OpMode::IDLE;
      }
      // Ensure client stays disabled until next update
      this->parent()->set_enabled(false);
      break;
    }

    case ESP_GATTC_SEARCH_CMPL_EVT: {
      ESP_LOGD(TAG, "Service discovery complete");

      // Find all characteristic handles
      auto *trigger_chr = this->parent()->get_characteristic(SERVICE_MAIN, CHAR_TRIGGER);
      if (trigger_chr != nullptr) this->trigger_handle_ = trigger_chr->handle;

      auto *result_chr = this->parent()->get_characteristic(SERVICE_DATA, CHAR_RESULT);
      if (result_chr != nullptr) this->result_handle_ = result_chr->handle;

      auto *cached_chr = this->parent()->get_characteristic(SERVICE_MAIN, CHAR_CACHED);
      if (cached_chr != nullptr) this->cached_handle_ = cached_chr->handle;

      auto *debug_chr = this->parent()->get_characteristic(SERVICE_MAIN, CHAR_DEBUG);
      if (debug_chr != nullptr) this->debug_handle_ = debug_chr->handle;

      auto *batt_a_chr = this->parent()->get_characteristic(SERVICE_MAIN, CHAR_BATT_A);
      if (batt_a_chr != nullptr) this->batt_a_handle_ = batt_a_chr->handle;

      auto *batt_b_chr = this->parent()->get_characteristic(SERVICE_MAIN, CHAR_BATT_B);
      if (batt_b_chr != nullptr) this->batt_b_handle_ = batt_b_chr->handle;

      if (this->mode_ == OpMode::CACHE_CONNECT) {
        // === CACHE READ MODE ===
        ESP_LOGI(TAG, "Reading cached measurement + battery candidates...");
        this->mode_ = OpMode::CACHE_READ;
        this->cache_reads_pending_ = 0;

        if (this->cached_handle_ != 0) {
          this->cache_reads_pending_++;
          esp_ble_gattc_read_char(this->parent()->get_gattc_if(),
            this->parent()->get_conn_id(), this->cached_handle_, ESP_GATT_AUTH_REQ_NONE);
        } else {
          ESP_LOGW(TAG, "Cached measurement characteristic (1962) not found");
        }

        if (this->batt_a_handle_ != 0) {
          this->cache_reads_pending_++;
          esp_ble_gattc_read_char(this->parent()->get_gattc_if(),
            this->parent()->get_conn_id(), this->batt_a_handle_, ESP_GATT_AUTH_REQ_NONE);
        }

        if (this->batt_b_handle_ != 0) {
          this->cache_reads_pending_++;
          esp_ble_gattc_read_char(this->parent()->get_gattc_if(),
            this->parent()->get_conn_id(), this->batt_b_handle_, ESP_GATT_AUTH_REQ_NONE);
        }

        if (this->cache_reads_pending_ == 0) {
          ESP_LOGW(TAG, "No characteristics to read, disconnecting");
          this->schedule_disconnect_();
        }
      } else if (this->mode_ == OpMode::MEASURE_CONNECT) {
        // === FRESH MEASUREMENT MODE ===
        ESP_LOGI(TAG, "Starting fresh measurement sequence...");
        this->mode_ = OpMode::MEASURE_SUBSCRIBE;

        // Subscribe to debug log (1969) - REQUIRED for radar to actually run
        if (this->debug_handle_ != 0) {
          ESP_LOGD(TAG, "Subscribing to debug log (1969)...");
          auto status = esp_ble_gattc_register_for_notify(
              this->parent()->get_gattc_if(), this->parent()->get_remote_bda(),
              this->debug_handle_);
          if (status != ESP_OK) {
            ESP_LOGW(TAG, "Failed to subscribe to debug: %d", status);
          }
        } else {
          ESP_LOGW(TAG, "Debug characteristic (1969) not found");
          // Try subscribing to result anyway
          if (this->result_handle_ != 0) {
            esp_ble_gattc_register_for_notify(
                this->parent()->get_gattc_if(), this->parent()->get_remote_bda(),
                this->result_handle_);
          }
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

      if (this->mode_ == OpMode::MEASURE_SUBSCRIBE) {
        if (param->reg_for_notify.handle == this->debug_handle_) {
          ESP_LOGD(TAG, "Subscribed to debug (1969), now subscribing to result (2014)...");
          if (this->result_handle_ != 0) {
            esp_ble_gattc_register_for_notify(
                this->parent()->get_gattc_if(), this->parent()->get_remote_bda(),
                this->result_handle_);
          }
        } else if (param->reg_for_notify.handle == this->result_handle_) {
          ESP_LOGD(TAG, "Subscribed to result (2014), now triggering measurement (1960)...");
          this->mode_ = OpMode::MEASURE_TRIGGER;
          if (this->trigger_handle_ != 0) {
            esp_ble_gattc_register_for_notify(
                this->parent()->get_gattc_if(), this->parent()->get_remote_bda(),
                this->trigger_handle_);
          }
        }
      } else if (this->mode_ == OpMode::MEASURE_TRIGGER) {
        if (param->reg_for_notify.handle == this->trigger_handle_) {
          ESP_LOGI(TAG, "Measurement triggered - waiting for radar...");
        }
      }
      break;
    }

    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.handle == this->trigger_handle_) {
        if (param->notify.value_len > 0) {
          uint8_t state = param->notify.value[0];
          if (state == 0x01) ESP_LOGD(TAG, "Radar: starting");
          else if (state == 0x02) ESP_LOGD(TAG, "Radar: measuring");
          else if (state == 0x05) ESP_LOGD(TAG, "Radar: complete");
        }
      } else if (param->notify.handle == this->debug_handle_) {
        // Log debug messages from the radar
        if (param->notify.value_len > 0) {
          std::string msg(reinterpret_cast<const char *>(param->notify.value),
                          param->notify.value_len);
          // Strip ;;/0 suffix
          auto pos = msg.find(";;/0");
          if (pos != std::string::npos) msg = msg.substr(0, pos);
          ESP_LOGD(TAG, "Radar debug: %s", msg.c_str());
        }
      } else if (param->notify.handle == this->result_handle_) {
        ESP_LOGI(TAG, "Distance result received");
        this->parse_distance_(param->notify.value, param->notify.value_len, false);
        this->publish_last_read_time_();
        this->schedule_disconnect_();
      }
      break;
    }

    case ESP_GATTC_READ_CHAR_EVT: {
      if (param->read.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Read failed for handle 0x%x, status=%d",
                 param->read.handle, param->read.status);
      } else if (param->read.handle == this->cached_handle_) {
        this->parse_distance_(param->read.value, param->read.value_len, true);
        this->publish_last_read_time_();
      } else if (param->read.handle == this->batt_a_handle_) {
        uint8_t val = param->read.value[0];
        ESP_LOGI(TAG, "Battery candidate 1930: raw=%u (0x%02X) — check Magnus app %%", val, val);
      } else if (param->read.handle == this->batt_b_handle_) {
        uint8_t val = param->read.value[0];
        ESP_LOGI(TAG, "Battery candidate 1967: raw=%u (0x%02X) — check Magnus app %%", val, val);
      }

      // In cache read mode, disconnect after all reads complete
      if (this->mode_ == OpMode::CACHE_READ) {
        this->cache_reads_pending_--;
        if (this->cache_reads_pending_ <= 0) {
          this->schedule_disconnect_();
        }
      }
      break;
    }

    default:
      break;
  }
}

void MagnusHawki::parse_distance_(const uint8_t *data, uint16_t length, bool from_cache) {
  std::string raw(reinterpret_cast<const char *>(data), length);
  ESP_LOGD(TAG, "Raw distance data (%s): %s", from_cache ? "cached" : "fresh", raw.c_str());

  float distance = 0;

  if (from_cache) {
    // Cache format: "00095 001 304 0" — first field is distance (0-padded)
    size_t first_space = raw.find(' ');
    if (first_space == std::string::npos) {
      ESP_LOGW(TAG, "Could not parse cached distance: no space found");
      return;
    }
    std::string distance_str = raw.substr(0, first_space);
    char *end;
    distance = std::strtof(distance_str.c_str(), &end);
    if (end == distance_str.c_str()) {
      ESP_LOGW(TAG, "Could not parse cached distance value: '%s'", distance_str.c_str());
      return;
    }
  } else {
    // Fresh format: "distance_mid  95/0" — value between last space and '/'
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
    distance = std::strtof(distance_str.c_str(), &end);
    if (end == distance_str.c_str()) {
      ESP_LOGW(TAG, "Could not parse distance value: '%s'", distance_str.c_str());
      return;
    }
  }

  if (distance >= 99999.0f) {
    ESP_LOGW(TAG, "No radar target detected (distance=%.0f), skipping publish", distance);
    return;
  }

  ESP_LOGI(TAG, "Oil distance: %.0f mm (%s)", distance, from_cache ? "cached" : "fresh");
  this->has_value_ = true;

  if (this->distance_sensor_ != nullptr) {
    this->distance_sensor_->publish_state(distance);
  }

  if (this->level_sensor_ != nullptr && this->tank_height_ > 0) {
    float oil_depth = this->tank_height_ - (distance - this->offset_);
    float level = (oil_depth / this->tank_height_) * 100.0f;
    if (level < 0.0f) level = 0.0f;
    if (level > 100.0f) level = 100.0f;
    ESP_LOGI(TAG, "Oil level: %.1f%% (depth=%.0fmm, tank=%.0fmm, offset=%.0fmm)",
             level, oil_depth, this->tank_height_, this->offset_);
    this->level_sensor_->publish_state(level);
  }
}

void MagnusHawki::publish_last_read_time_() {
  if (this->last_read_sensor_ == nullptr)
    return;

  time_t now = ::time(nullptr);
  if (now < 1600000000) {
    // Time not synced yet
    this->last_read_sensor_->publish_state("unknown");
    return;
  }

  auto esptime = ESPTime::from_epoch_local(now);
  char buf[20];
  esptime.strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S");
  ESP_LOGD(TAG, "Last read time: %s", buf);
  this->last_read_sensor_->publish_state(buf);
}

void MagnusHawki::arm_watchdog_() {
  this->cancel_timeout("watchdog");
  this->set_timeout("watchdog", OPERATION_TIMEOUT_MS, [this]() {
    if (this->mode_ != OpMode::IDLE) {
      ESP_LOGW(TAG, "Operation timed out after %u ms, forcing disconnect", OPERATION_TIMEOUT_MS);
      this->mode_ = OpMode::IDLE;
      this->do_disconnect_();

      if (this->retry_count_ < MAX_RETRIES) {
        this->retry_count_++;
        ESP_LOGI(TAG, "Scheduling retry %d/%d in 30s", this->retry_count_, MAX_RETRIES);
        this->set_timeout("retry", RETRY_DELAY_MS, [this]() { this->update(); });
      } else {
        ESP_LOGW(TAG, "Max retries reached, waiting for next update cycle");
        this->retry_count_ = 0;
      }
    }
  });
}

void MagnusHawki::schedule_disconnect_() {
  this->cancel_timeout("watchdog");
  ESP_LOGI(TAG, "Operation complete, disconnecting...");
  this->retry_count_ = 0;
  this->mode_ = OpMode::DISCONNECTING;
  this->do_disconnect_();
}

void MagnusHawki::do_disconnect_() {
  this->mode_ = OpMode::IDLE;
  this->parent()->set_enabled(false);
}

}  // namespace magnus_hawki
}  // namespace esphome

#endif  // USE_ESP32
