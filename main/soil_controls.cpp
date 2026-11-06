#include "soil_controls.h"
#include "soil_moisture.h"
#include "soil_settings.h"

#include <cstdio>
#include <atomic>
#include <esp_log.h>
#include <esp_pm.h>
#include <initializer_list>
#include <app/clusters/mode-select-server/supported-modes-manager.h>
#include <platform/CHIPDeviceLayer.h>

namespace {
constexpr char TAG[] = "soil_controls";
namespace Mode = chip::app::Clusters::ModeSelect;
using Option = Mode::Structs::ModeOptionStruct::Type;
using Status = chip::Protocols::InteractionModel::Status;
uint16_t s_actions = 0, s_interval = 0, s_water = 0, s_wet = 0, s_samples = 0, s_maintenance = 0;
std::atomic<bool> s_active{false};
esp_pm_lock_handle_t s_maintenance_lock = nullptr;
bool s_maintenance_on = false;

Option make_option(const char *label, uint8_t mode)
{
    Option option;
    option.label = chip::CharSpan::fromCharString(label);
    option.mode = mode;
    return option;
}

class ModeManager final : public Mode::SupportedModesManager {
public:
    Option actions[base::soil::kModeCount];
    Option intervals[base::soil::kIntervalCount];
    Option water[base::soil::kWaterBelowCount];
    Option wet[base::soil::kWetAboveCount];
    Option samples[base::soil::kSamplesCount];
    ModeManager()
    {
        for (uint8_t i = 0; i < base::soil::kWaterBelowCount; ++i) water[i] = make_option(base::soil::kWaterBelow[i].label, i);
        for (uint8_t i = 0; i < base::soil::kWetAboveCount; ++i) wet[i] = make_option(base::soil::kWetAbove[i].label, i);
        for (uint8_t i = 0; i < base::soil::kSamplesCount; ++i) samples[i] = make_option(base::soil::kSamples[i].label, i);
        for (uint8_t i = 0; i < base::soil::kModeCount; ++i)
            actions[i] = make_option(base::soil::mode_label(i), i);
        for (uint8_t i = 0; i < base::soil::kIntervalCount; ++i)
            intervals[i] = make_option(base::soil::kIntervals[i].label, i);
    }
    ModeOptionsProvider getModeOptionsProvider(chip::EndpointId endpoint) const override
    {
        if (endpoint == s_actions) return {actions, actions + base::soil::kModeCount};
        if (endpoint == s_interval) return {intervals, intervals + base::soil::kIntervalCount};
        if (s_water != 0 && endpoint == s_water) return {water, water + base::soil::kWaterBelowCount};
        if (s_wet != 0 && endpoint == s_wet) return {wet, wet + base::soil::kWetAboveCount};
        if (s_samples != 0 && endpoint == s_samples) return {samples, samples + base::soil::kSamplesCount};
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

void report_result(intptr_t result)
{
    const uint8_t mode = base::soil::result_mode(static_cast<base::soil::Result>(result));
    ESP_LOGI(TAG, "Requested operation finished: %s", base::soil::mode_label(mode));
    report_mode(s_actions, mode);
}

void report_setting(intptr_t raw)
{
    using base::soil::Setting;
    const auto setting = static_cast<Setting>(raw);
    int mode = -1;
    uint16_t endpoint = 0;
    switch (setting) {
    case Setting::Interval: endpoint = s_interval; mode = base::soil::interval_mode(base::soil::interval_minutes()); break;
    case Setting::WaterBelow: endpoint = s_water; mode = base::soil::choice_mode(base::soil::kWaterBelow, base::soil::water_below_percent()); break;
    case Setting::WetAbove: endpoint = s_wet; mode = base::soil::choice_mode(base::soil::kWetAbove, base::soil::wet_above_percent()); break;
    case Setting::Samples: endpoint = s_samples; mode = base::soil::choice_mode(base::soil::kSamples, base::soil::samples()); break;
    default: break;
    }
    if (endpoint != 0 && mode >= 0) report_mode(endpoint, static_cast<uint8_t>(mode));
}

void report_maintenance(intptr_t)
{
    if (s_maintenance == 0) return;
    auto value = esp_matter_bool(s_maintenance_on);
    const esp_err_t err = esp_matter::attribute::report(s_maintenance, chip::app::Clusters::OnOff::Id,
                                                        chip::app::Clusters::OnOff::Attributes::OnOff::Id, &value);
    if (err != ESP_OK) ESP_LOGE(TAG, "Maintenance report failed: %s", esp_err_to_name(err));
}

// Maintenance mode blocks automatic Light-sleep, so the USB console stays readable and
// the device answers without sleep delays. Never stored: every restart begins with it off.
esp_err_t set_maintenance(bool on)
{
    if (s_maintenance_lock == nullptr || on == s_maintenance_on) return ESP_OK;
    const esp_err_t err = on ? esp_pm_lock_acquire(s_maintenance_lock) : esp_pm_lock_release(s_maintenance_lock);
    if (err == ESP_OK) {
        s_maintenance_on = on;
        ESP_LOGW(TAG, "Maintenance mode %s: automatic Light-sleep %s", on ? "ON" : "OFF", on ? "blocked" : "allowed");
    }
    return err;
}

void initialize_controls(intptr_t)
{
    report_mode(s_actions, 0);
    using base::soil::Setting;
    for (Setting s : {Setting::Interval, Setting::WaterBelow, Setting::WetAbove, Setting::Samples})
        report_setting(static_cast<intptr_t>(s));
    report_maintenance(0); // the stored OnOff attribute may say on; the device starts off
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

esp_err_t base::controls::create_settings_endpoints(esp_matter::node_t *node)
{
    using namespace base::soil;
    esp_err_t err = create_mode_endpoint(node, "Gießen nötig unter",
                                         static_cast<uint8_t>(choice_mode(kWaterBelow, water_below_percent())), s_water);
    if (err == ESP_OK)
        err = create_mode_endpoint(node, "Sehr feucht ab",
                                   static_cast<uint8_t>(choice_mode(kWetAbove, wet_above_percent())), s_wet);
    if (err == ESP_OK)
        err = create_mode_endpoint(node, "Anzahl Einzelmessungen",
                                   static_cast<uint8_t>(choice_mode(kSamples, samples())), s_samples);
    if (err != ESP_OK) return err;
    esp_matter::endpoint::on_off_plugin_unit::config_t plug{};
    plug.on_off.on_off = false;
    auto *endpoint = esp_matter::endpoint::on_off_plugin_unit::create(node, &plug, esp_matter::ENDPOINT_FLAG_NONE, nullptr);
    if (endpoint == nullptr) return ESP_ERR_NO_MEM;
    s_maintenance = esp_matter::endpoint::get_id(endpoint);
    ESP_LOGI(TAG, "Settings on endpoints %u-%u, maintenance switch on endpoint %u", s_water, s_samples, s_maintenance);
    return esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "maintenance", &s_maintenance_lock);
}

void base::controls::start()
{
    s_active = true;
    chip::DeviceLayer::PlatformMgr().ScheduleWork(initialize_controls, 0);
}

esp_err_t base::controls::attribute_changed(esp_matter::attribute::callback_type_t type, uint16_t endpoint,
                                           uint32_t cluster, uint32_t attribute, esp_matter_attr_val_t *value)
{
    if (!s_active || type != esp_matter::attribute::POST_UPDATE || value == nullptr) return ESP_OK;
    if (s_maintenance != 0 && endpoint == s_maintenance && cluster == chip::app::Clusters::OnOff::Id &&
        attribute == chip::app::Clusters::OnOff::Attributes::OnOff::Id) {
        const esp_err_t err = set_maintenance(value->val.b);
        if (err != ESP_OK) ESP_LOGE(TAG, "Maintenance mode not changed: %s", esp_err_to_name(err));
        return err;
    }
    if (cluster != Mode::Id || attribute != Mode::Attributes::CurrentMode::Id) return ESP_OK;
    const uint8_t mode = value->val.u8;
    if (endpoint == s_actions && mode > 0 && mode < base::soil::kActionCount) {
        const esp_err_t err = mode == base::soil::kMeasureMode ?
            base::soil::request(base::soil::Request::Measure) :
            base::soil::request(base::soil::action_is_dry(mode) ? base::soil::Request::Dry : base::soil::Request::Wet,
                                base::soil::action_sensor(mode));
        if (err != ESP_OK) operation_finished(base::soil::Result::Busy);
        return err;
    }
    if (endpoint == s_interval && mode < base::soil::kIntervalCount) {
        const esp_err_t err = base::soil::set_interval_minutes(base::soil::kIntervals[mode].minutes);
        if (err != ESP_OK) setting_changed(base::soil::Setting::Interval);
        return err;
    }
    using base::soil::Setting;
    Setting setting = Setting::None;
    uint8_t chosen = 0;
    if (s_water != 0 && endpoint == s_water && mode < base::soil::kWaterBelowCount) {
        setting = Setting::WaterBelow; chosen = base::soil::kWaterBelow[mode].value;
    } else if (s_wet != 0 && endpoint == s_wet && mode < base::soil::kWetAboveCount) {
        setting = Setting::WetAbove; chosen = base::soil::kWetAbove[mode].value;
    } else if (s_samples != 0 && endpoint == s_samples && mode < base::soil::kSamplesCount) {
        setting = Setting::Samples; chosen = base::soil::kSamples[mode].value;
    }
    if (setting == Setting::None) return ESP_OK;
    const esp_err_t err = base::soil::change_setting(setting, chosen);
    if (err != ESP_OK) setting_changed(setting);
    return err;
}

void base::controls::operation_finished(base::soil::Result result)
{
    if (chip::DeviceLayer::PlatformMgr().ScheduleWork(report_result, static_cast<intptr_t>(result)) != CHIP_NO_ERROR)
        ESP_LOGE(TAG, "Could not schedule action confirmation");
}

void base::controls::setting_changed(base::soil::Setting setting)
{
    if (chip::DeviceLayer::PlatformMgr().ScheduleWork(report_setting, static_cast<intptr_t>(setting)) != CHIP_NO_ERROR)
        ESP_LOGE(TAG, "Could not schedule setting confirmation");
}
