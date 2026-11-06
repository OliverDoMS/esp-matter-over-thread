#include "sensors.h"
#include "soil_moisture.h"
#include "soil_controls.h"

#include <esp_log.h>
#include <sdkconfig.h>

#if CONFIG_BASE_RAIN_SENSOR
#include "rain_logic.h"

#include <driver/gpio.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_check.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <platform/CHIPDeviceLayer.h>
#endif

namespace {
constexpr char TAG[] = "sensors";
#if CONFIG_BASE_SOIL_MOISTURE
uint16_t s_soil_endpoint_ids[base::soil::kSensorCount] = {};
#if CONFIG_BASE_SOIL_RAW_ENDPOINTS
uint16_t s_soil_raw_endpoint_ids[base::soil::kSensorCount] = {};
#endif
uint16_t s_soil_moist_endpoint_ids[base::soil::kSensorCount] = {};
#endif
#if CONFIG_BASE_TEST_CONTACT
uint16_t s_endpoint_id = 0;
bool s_closed = true;
#endif

#if CONFIG_BASE_RAIN_SENSOR
constexpr int kMaxSamples = 30;
uint16_t s_rain_endpoint_id = 0;
bool s_raining = false;
bool s_published = false;
adc_oneshot_unit_handle_t s_adc = nullptr;
adc_cali_handle_t s_cali = nullptr;
adc_channel_t s_channel;
TaskHandle_t s_task = nullptr;

// Runs with the Matter stack lock held (via ScheduleWork).
void publish_rain(intptr_t raining)
{
    auto value = esp_matter_bool(raining != 0);
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_matter::attribute::update(
        s_rain_endpoint_id, chip::app::Clusters::BooleanState::Id,
        chip::app::Clusters::BooleanState::Attributes::StateValue::Id, &value));
}

esp_err_t init_adc()
{
    adc_unit_t unit;
    ESP_RETURN_ON_ERROR(adc_oneshot_io_to_channel(CONFIG_BASE_RAIN_ADC_GPIO, &unit, &s_channel), TAG,
                        "GPIO %d is not an ADC pin", CONFIG_BASE_RAIN_ADC_GPIO);
    adc_oneshot_unit_init_cfg_t unit_cfg{};
    unit_cfg.unit_id = unit;
    ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&unit_cfg, &s_adc), TAG, "ADC unit failed");
    adc_oneshot_chan_cfg_t chan_cfg{};
    chan_cfg.atten = ADC_ATTEN_DB_12;
    chan_cfg.bitwidth = ADC_BITWIDTH_DEFAULT;
    ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(s_adc, s_channel, &chan_cfg), TAG, "ADC channel failed");
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cali_cfg{};
    cali_cfg.unit_id = unit;
    cali_cfg.chan = s_channel;
    cali_cfg.atten = ADC_ATTEN_DB_12;
    cali_cfg.bitwidth = ADC_BITWIDTH_DEFAULT;
    if (adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_cali) != ESP_OK) {
        ESP_LOGW(TAG, "No ADC calibration; using a rough raw-to-mV estimate");
    }
#endif
    return ESP_OK;
}

// Powers the sensor only for the measurement and returns the filtered voltage in mV, or -1.
int measure_mv()
{
    int samples[kMaxSamples];
    size_t count = 0;
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_set_level(static_cast<gpio_num_t>(CONFIG_BASE_RAIN_POWER_GPIO), 1));
    vTaskDelay(pdMS_TO_TICKS(CONFIG_BASE_RAIN_SETTLE_MS));
    for (int i = 0; i < CONFIG_BASE_RAIN_SAMPLES && i < kMaxSamples; i++) {
        int raw = 0;
        if (adc_oneshot_read(s_adc, s_channel, &raw) != ESP_OK) continue;
        int mv = 0;
        if (s_cali == nullptr || adc_cali_raw_to_voltage(s_cali, raw, &mv) != ESP_OK) {
            mv = raw * 2450 / 4095; // rough estimate for 12 dB
        }
        samples[count++] = mv;
        vTaskDelay(1);
    }
    // Sensor off right away: no current in standby, no electrode corrosion.
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_set_level(static_cast<gpio_num_t>(CONFIG_BASE_RAIN_POWER_GPIO), 0));
    const int mv = base::rain::trimmed_mean_mv(samples, count);
    return mv > 0 ? mv : -1;
}

void measure_and_publish()
{
    const int mv = measure_mv();
    if (mv < 0) {
        ESP_LOGW(TAG, "Rain measurement failed; keeping the last state");
        return;
    }
    const bool raining = base::rain::next_state(s_raining, mv, CONFIG_BASE_RAIN_WET_BELOW_MV,
                                                CONFIG_BASE_RAIN_DRY_ABOVE_MV);
    ESP_LOGI(TAG, "Rain sensor %d mV -> %s", mv, raining ? "RAIN" : "dry");
    if (raining != s_raining || !s_published) {
        s_raining = raining;
        s_published = true;
        chip::DeviceLayer::PlatformMgr().ScheduleWork(publish_rain, raining ? 1 : 0);
    }
}

void rain_task(void *)
{
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        measure_and_publish();
    }
}

void timer_cb(void *)
{
    xTaskNotifyGive(s_task);
}
#endif
}

esp_err_t base::sensors::create_endpoints(esp_matter::node_t *node)
{
    if (node == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
#if CONFIG_BASE_RAIN_SENSOR
    esp_matter::endpoint::rain_sensor::config_t rain_config{};
    rain_config.boolean_state.state_value = false;
    rain_config.identify.identify_type = 0; // No visible identification indicator.
    auto *rain_endpoint = esp_matter::endpoint::rain_sensor::create(node, &rain_config, esp_matter::ENDPOINT_FLAG_NONE, nullptr);
    if (rain_endpoint == nullptr) {
        return ESP_ERR_NO_MEM;
    }
    s_rain_endpoint_id = esp_matter::endpoint::get_id(rain_endpoint);
    ESP_LOGI(TAG, "Rain sensor on endpoint %u", s_rain_endpoint_id);
#endif
#if CONFIG_BASE_SOIL_MOISTURE
    esp_matter::endpoint::humidity_sensor::config_t soil_config{};
    soil_config.identify.identify_type = 0;
    soil_config.relative_humidity_measurement.min_measured_value = static_cast<uint16_t>(0);
    soil_config.relative_humidity_measurement.max_measured_value = static_cast<uint16_t>(10000);
    for (uint8_t i = 0; i < base::soil::kSensorCount; ++i) {
        // Home Assistant only creates an entity when the value is not null at its interview.
        const int initial = base::soil::initial_hundredths(i);
        soil_config.relative_humidity_measurement.measured_value = initial < 0 ?
            nullable<uint16_t>() : nullable<uint16_t>(static_cast<uint16_t>(initial));
        auto *soil_endpoint = esp_matter::endpoint::humidity_sensor::create(
            node, &soil_config, esp_matter::ENDPOINT_FLAG_NONE, nullptr);
        if (soil_endpoint == nullptr) return ESP_ERR_NO_MEM;
        s_soil_endpoint_ids[i] = esp_matter::endpoint::get_id(soil_endpoint);
        ESP_LOGI(TAG, "Relative soil moisture %d on endpoint %u (Matter humidity compatibility)",
                 i + 1, s_soil_endpoint_ids[i]);
    }
    const esp_err_t controls_error = base::controls::create_endpoints(node);
    if (controls_error != ESP_OK) return controls_error;
#if CONFIG_BASE_SOIL_RAW_ENDPOINTS
    // Created after the controls, so endpoints 1-5 keep their numbers from earlier versions.
    esp_matter::endpoint::pressure_sensor::config_t raw_config{};
    raw_config.identify.identify_type = 0;
    raw_config.pressure_measurement.pressure_measured_value = nullable<int16_t>(static_cast<int16_t>(0));
    raw_config.pressure_measurement.pressure_min_measured_value = nullable<int16_t>(static_cast<int16_t>(0));
    raw_config.pressure_measurement.pressure_max_measured_value = nullable<int16_t>(static_cast<int16_t>(4095));
    for (uint8_t i = 0; i < base::soil::kSensorCount; ++i) {
        auto *raw_endpoint = esp_matter::endpoint::pressure_sensor::create(
            node, &raw_config, esp_matter::ENDPOINT_FLAG_NONE, nullptr);
        if (raw_endpoint == nullptr) return ESP_ERR_NO_MEM;
        s_soil_raw_endpoint_ids[i] = esp_matter::endpoint::get_id(raw_endpoint);
        ESP_LOGI(TAG, "Voltage (mV) of soil sensor %d on endpoint %u (Matter pressure compatibility)",
                 i + 1, s_soil_raw_endpoint_ids[i]);
    }
#endif
    // 0.7.0: added after every earlier endpoint, so endpoints 1-8 keep their numbers.
    // Water Leak Detector: Home Assistant shows a moisture binary sensor, "moist" or "dry";
    // dry means watering needed. Starts moist, so no alarm appears before the first reading.
    esp_matter::endpoint::water_leak_detector::config_t moist_config{};
    moist_config.identify.identify_type = 0;
    moist_config.boolean_state.state_value = true;
    for (uint8_t i = 0; i < base::soil::kSensorCount; ++i) {
        auto *moist_endpoint = esp_matter::endpoint::water_leak_detector::create(
            node, &moist_config, esp_matter::ENDPOINT_FLAG_NONE, nullptr);
        if (moist_endpoint == nullptr) return ESP_ERR_NO_MEM;
        s_soil_moist_endpoint_ids[i] = esp_matter::endpoint::get_id(moist_endpoint);
        ESP_LOGI(TAG, "Soil sensor %d moist/dry (watering) on endpoint %u", i + 1, s_soil_moist_endpoint_ids[i]);
    }
    const esp_err_t settings_error = base::controls::create_settings_endpoints(node);
    if (settings_error != ESP_OK) return settings_error;
#endif
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

esp_err_t base::sensors::start()
{
#if CONFIG_BASE_RAIN_SENSOR
    gpio_config_t power{};
    power.pin_bit_mask = 1ULL << CONFIG_BASE_RAIN_POWER_GPIO;
    power.mode = GPIO_MODE_OUTPUT;
    ESP_RETURN_ON_ERROR(gpio_config(&power), TAG, "Sensor power GPIO failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(static_cast<gpio_num_t>(CONFIG_BASE_RAIN_POWER_GPIO), 0), TAG, "Sensor power off failed");
    ESP_RETURN_ON_ERROR(init_adc(), TAG, "ADC init failed");
    ESP_RETURN_ON_FALSE(xTaskCreate(rain_task, "rain", 3072, nullptr, 3, &s_task) == pdPASS, ESP_ERR_NO_MEM, TAG,
                        "Rain task failed");
    esp_timer_create_args_t args{};
    args.callback = timer_cb;
    args.name = "rain";
    esp_timer_handle_t timer = nullptr;
    ESP_RETURN_ON_ERROR(esp_timer_create(&args, &timer), TAG, "Timer create failed");
    ESP_RETURN_ON_ERROR(esp_timer_start_periodic(timer, CONFIG_BASE_RAIN_INTERVAL_S * 1000000ULL), TAG,
                        "Timer start failed");
    xTaskNotifyGive(s_task); // First measurement right after startup
#endif
#if CONFIG_BASE_SOIL_MOISTURE
#if CONFIG_BASE_SOIL_RAW_ENDPOINTS
    const esp_err_t err = base::soil::start(s_soil_endpoint_ids, s_soil_raw_endpoint_ids, s_soil_moist_endpoint_ids);
#else
    const esp_err_t err = base::soil::start(s_soil_endpoint_ids, nullptr, s_soil_moist_endpoint_ids);
#endif
    if (err == ESP_OK) base::controls::start();
    return err;
#else
    return ESP_OK;
#endif
}

esp_err_t base::sensors::toggle_test_contact()
{
#if CONFIG_BASE_RAIN_SENSOR
    if (s_task == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    xTaskNotifyGive(s_task); // Measure right now
#endif
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
