#pragma once
#include "soil_settings.h"
#include <esp_matter.h>

namespace base::controls {
// Endpoints 4 and 5 (actions, interval); kept first so their numbers never change.
esp_err_t create_endpoints(esp_matter::node_t *node);
// Added in 0.7.0 after all earlier endpoints: thresholds, sample count, maintenance switch.
esp_err_t create_settings_endpoints(esp_matter::node_t *node);
void start();
esp_err_t attribute_changed(esp_matter::attribute::callback_type_t type, uint16_t endpoint,
                            uint32_t cluster, uint32_t attribute, esp_matter_attr_val_t *value);
// Called by the ADC worker; actual Matter writes are scheduled onto its event loop.
// The action selection then shows the result until the next action.
void operation_finished(base::soil::Result result);
// Confirms a stored (or rejected) setting back to Home Assistant.
void setting_changed(base::soil::Setting setting);
}
