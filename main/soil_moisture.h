#pragma once
#include <esp_err.h>
#include <cstdint>

namespace base::soil {
enum class Request : uint32_t { Measure = 1, Dry = 2, Wet = 4 };
esp_err_t load_settings();
uint16_t interval_minutes();
esp_err_t set_interval_minutes(uint16_t minutes);
esp_err_t start(uint16_t endpoint_id);
esp_err_t request(Request action);
}
