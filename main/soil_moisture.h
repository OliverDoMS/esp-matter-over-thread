#pragma once
#include "soil_math.h"
#include "soil_settings.h"
#include <esp_err.h>
#include <cstdint>

namespace base::soil {
enum class Request : uint32_t { Measure = 1, Dry = 2, Wet = 4 };
esp_err_t load_settings();
// Both references valid; call after load_settings().
bool calibrated(uint8_t sensor);
// Value to show before the first measurement: a calibration placeholder, or -1 (null).
int initial_hundredths(uint8_t sensor);
uint16_t interval_minutes();
esp_err_t set_interval_minutes(uint16_t minutes);
// Each array holds kSensorCount entries. raw_endpoint_ids (sensor voltage) and
// moist_endpoint_ids ("soil moist" binary sensors) may be nullptr.
esp_err_t start(const uint16_t *endpoint_ids, const uint16_t *raw_endpoint_ids, const uint16_t *moist_endpoint_ids);
// Shared thresholds and sample count; changes go through the ADC worker and NVS.
uint8_t water_below_percent();
uint8_t wet_above_percent();
uint8_t samples();
esp_err_t change_setting(Setting setting, uint8_t value);
// Measure covers all sensors; Dry and Wet need the zero-based sensor they calibrate.
esp_err_t request(Request action, uint8_t sensor = kAllSensors);
}
