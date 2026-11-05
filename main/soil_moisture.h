#pragma once
#include "soil_settings.h"
#include <esp_err.h>
#include <cstdint>

namespace base::soil {
enum class Request : uint32_t { Measure = 1, Dry = 2, Wet = 4 };
esp_err_t load_settings();
uint16_t interval_minutes();
esp_err_t set_interval_minutes(uint16_t minutes);
// endpoint_ids holds kSensorCount entries.
esp_err_t start(const uint16_t *endpoint_ids);
// Measure covers all sensors; Dry and Wet need the zero-based sensor they calibrate.
esp_err_t request(Request action, uint8_t sensor = kAllSensors);
}
