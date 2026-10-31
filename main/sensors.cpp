#include "sensors.h"

#include <esp_log.h>
#include <sdkconfig.h>

namespace {
constexpr char TAG[] = "sensors";
#if CONFIG_BASE_TEST_CONTACT
uint16_t s_endpoint_id = 0;
bool s_closed = true;
#endif
}

esp_err_t base::sensors::create_endpoints(esp_matter::node_t *node)
{
    if (node == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
#if CONFIG_BASE_TEST_CONTACT
    esp_matter::endpoint::contact_sensor::config_t config{};
    config.boolean_state.state_value = s_closed;
    config.identify.identify_type = 0; // No visible identification indicator.
    auto *endpoint = esp_matter::endpoint::contact_sensor::create(node, &config, esp_matter::ENDPOINT_FLAG_NONE, nullptr);
    if (endpoint == nullptr) {
        return ESP_ERR_NO_MEM;
    }
    s_endpoint_id = esp_matter::endpoint::get_id(endpoint);
    ESP_LOGI(TAG, "TEST contact on endpoint %u; not a physical sensor", s_endpoint_id);
#endif
    return ESP_OK;
}

esp_err_t base::sensors::toggle_test_contact()
{
#if CONFIG_BASE_TEST_CONTACT
    if (s_endpoint_id == 0) {
        return ESP_ERR_INVALID_STATE;
    }
    const bool next_closed = !s_closed;
    auto value = esp_matter_bool(next_closed);
    const esp_err_t err = esp_matter::attribute::update(
        s_endpoint_id, chip::app::Clusters::BooleanState::Id,
        chip::app::Clusters::BooleanState::Attributes::StateValue::Id, &value);
    if (err == ESP_OK) {
        s_closed = next_closed;
        ESP_LOGI(TAG, "TEST contact: %s", s_closed ? "closed" : "open");
    }
    return err;
#else
    return ESP_OK;
#endif
}
