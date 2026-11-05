#pragma once

#include <esp_err.h>
#include <esp_matter.h>

namespace base::sensors {
// Extend here with physical sensor drivers and their Matter endpoints.
esp_err_t create_endpoints(esp_matter::node_t *node);
// Starts the periodic measurement; call after esp_matter::start().
esp_err_t start();
// Short BOOT press: toggles the virtual test contact, or measures the rain sensor right now.
// The soil variant handles its button actions in soil_controls.
// All publishing functions run on the Matter event loop, never in a GPIO ISR.
esp_err_t toggle_test_contact();
}
