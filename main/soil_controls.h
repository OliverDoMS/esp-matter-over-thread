#pragma once
#include <esp_matter.h>

namespace base::controls {
esp_err_t create_endpoints(esp_matter::node_t *node);
void start();
esp_err_t attribute_changed(esp_matter::attribute::callback_type_t type, uint16_t endpoint,
                            uint32_t cluster, uint32_t attribute, esp_matter_attr_val_t *value);
// Called by the ADC worker; actual Matter writes are scheduled onto its event loop.
void operation_finished(bool success);
void interval_changed();
}
