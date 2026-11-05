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
// Both arrays hold kSensorCount entries. raw_endpoint_ids may be nullptr (raw values off).
esp_err_t start(const uint16_t *endpoint_ids, const uint16_t *raw_endpoint_ids);
// Measure covers all sensors; Dry and Wet need the zero-based sensor they calibrate.
esp_err_t request(Request action, uint8_t sensor = kAllSensors);
}
