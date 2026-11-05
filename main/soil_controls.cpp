#include "soil_controls.h"
#include "soil_moisture.h"
#include "soil_settings.h"

#include <cstdio>
#include <atomic>
#include <esp_log.h>
#include <app/clusters/mode-select-server/supported-modes-manager.h>
#include <platform/CHIPDeviceLayer.h>

namespace {
constexpr char TAG[] = "soil_controls";
namespace Mode = chip::app::Clusters::ModeSelect;
using Option = Mode::Structs::ModeOptionStruct::Type;
using Status = chip::Protocols::InteractionModel::Status;
uint16_t s_actions = 0, s_interval = 0;
std::atomic<bool> s_active{false};

Option make_option(const char *label, uint8_t mode)
{
    Option option;
    option.label = chip::CharSpan::fromCharString(label);
    option.mode = mode;
    return option;
}

class ModeManager final : public Mode::SupportedModesManager {
public:
    Option actions[base::soil::kActionCount];
    Option intervals[base::soil::kIntervalCount];
    ModeManager()
    {
        for (uint8_t i = 0; i < base::soil::kActionCount; ++i)
            actions[i] = make_option(base::soil::kActionLabels[i], i);
        for (uint8_t i = 0; i < base::soil::kIntervalCount; ++i)
            intervals[i] = make_option(base::soil::kIntervals[i].label, i);
    }
    ModeOptionsProvider getModeOptionsProvider(chip::EndpointId endpoint) const override
    {
        if (endpoint == s_actions) return {actions, actions + base::soil::kActionCount};
        if (endpoint == s_interval) return {intervals, intervals + base::soil::kIntervalCount};
        return {};
    }
    Status getModeOptionByMode(chip::EndpointId endpoint, uint8_t mode, const Option **out) const override
    {
        const auto options = getModeOptionsProvider(endpoint);
        for (const auto &option : options) {
            if (option.mode == mode) { *out = &option; return Status::Success; }
        }
        return Status::InvalidCommand;
    }
} s_modes;

void report_mode(uint16_t endpoint, uint8_t mode)
{
    auto value = esp_matter_uint8(mode);
    const esp_err_t err = esp_matter::attribute::report(endpoint, Mode::Id, Mode::Attributes::CurrentMode::Id, &value);
    if (err != ESP_OK) ESP_LOGE(TAG, "Mode report failed: %s", esp_err_to_name(err));
}

void report_result(intptr_t success)
{
    ESP_LOGI(TAG, "Requested operation %s", success ? "completed" : "FAILED; calibration unchanged if rejected");
    report_mode(s_actions, 0);
}

void report_interval(intptr_t)
{
    const int mode = base::soil::interval_mode(base::soil::interval_minutes());
    if (mode >= 0) report_mode(s_interval, static_cast<uint8_t>(mode));
}

void initialize_controls(intptr_t)
{
    report_mode(s_actions, 0);
    report_interval(0);
}

esp_err_t create_mode_endpoint(esp_matter::node_t *node, const char *description, uint8_t initial, uint16_t &id)
{
    esp_matter::endpoint::mode_select_device::config_t config{};
    std::snprintf(config.mode_select.mode_select_description, sizeof(config.mode_select.mode_select_description), "%s", description);
    config.mode_select.current_mode = initial;
    config.mode_select.delegate = &s_modes;
    auto *endpoint = esp_matter::endpoint::mode_select_device::create(node, &config, esp_matter::ENDPOINT_FLAG_NONE, nullptr);
    if (endpoint == nullptr) return ESP_ERR_NO_MEM;
    id = esp_matter::endpoint::get_id(endpoint);
    return ESP_OK;
}
}

esp_err_t base::controls::create_endpoints(esp_matter::node_t *node)
{
    esp_err_t err = create_mode_endpoint(node, "Bodenfeuchte bedienen", 0, s_actions);
    if (err != ESP_OK) return err;
    const int mode = base::soil::interval_mode(base::soil::interval_minutes());
    return create_mode_endpoint(node, "Messintervall", static_cast<uint8_t>(mode), s_interval);
}

void base::controls::start()
{
    s_active = true;
    chip::DeviceLayer::PlatformMgr().ScheduleWork(initialize_controls, 0);
}

esp_err_t base::controls::attribute_changed(esp_matter::attribute::callback_type_t type, uint16_t endpoint,
                                           uint32_t cluster, uint32_t attribute, esp_matter_attr_val_t *value)
{
    if (!s_active || type != esp_matter::attribute::POST_UPDATE || cluster != Mode::Id ||
        attribute != Mode::Attributes::CurrentMode::Id || value == nullptr) return ESP_OK;
    const uint8_t mode = value->val.u8;
    if (endpoint == s_actions && mode > 0 && mode < base::soil::kActionCount) {
        const esp_err_t err = mode == base::soil::kMeasureMode ?
            base::soil::request(base::soil::Request::Measure) :
            base::soil::request(base::soil::action_is_dry(mode) ? base::soil::Request::Dry : base::soil::Request::Wet,
                                base::soil::action_sensor(mode));
        if (err != ESP_OK) operation_finished(false);
        return err;
    }
    if (endpoint == s_interval && mode < base::soil::kIntervalCount) {
        const esp_err_t err = base::soil::set_interval_minutes(base::soil::kIntervals[mode].minutes);
        if (err != ESP_OK) interval_changed();
        return err;
    }
    return ESP_OK;
}

void base::controls::operation_finished(bool success)
{
    if (chip::DeviceLayer::PlatformMgr().ScheduleWork(report_result, success ? 1 : 0) != CHIP_NO_ERROR)
        ESP_LOGE(TAG, "Could not schedule action confirmation");
}

void base::controls::interval_changed()
{
    if (chip::DeviceLayer::PlatformMgr().ScheduleWork(report_interval, 0) != CHIP_NO_ERROR)
        ESP_LOGE(TAG, "Could not schedule interval confirmation");
}
