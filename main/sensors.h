#pragma once

#include <esp_err.h>
#include <esp_matter.h>

namespace base::sensors {
// Extend here with physical sensor drivers and their Matter endpoints.
esp_err_t create_endpoints(esp_matter::node_t *node);
esp_err_t start();
// All publishing functions run on the Matter event loop, never in a GPIO ISR.
esp_err_t toggle_test_contact();
}
