#include "soil_moisture.h"
#include "soil_math.h"

#include <algorithm>
#include <esp_adc/adc_oneshot.h>
#include <esp_log.h>
#include <esp_matter.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs.h>
#include <platform/CHIPDeviceLayer.h>
#include <app/icd/server/ICDNotifier.h>
#include <sdkconfig.h>

#if CONFIG_BASE_SOIL_MOISTURE
namespace {
constexpr char TAG[] = "soil";
TaskHandle_t s_task = nullptr;
adc_oneshot_unit_handle_t s_adc = nullptr;
adc_channel_t s_channel;
uint16_t s_endpoint;
nvs_handle_t s_nvs;
int32_t s_dry = -1, s_wet = -1;

void publish(intptr_t reading)
{
    // Scheduled onto the Matter loop; never publish directly from the ADC task.
    nullable<uint16_t> measured;
    if (reading >= 0) measured = static_cast<uint16_t>(reading);
    auto value = esp_matter_nullable_uint16(measured);
    chip::app::ICDNotifier::GetInstance().NotifyNetworkActivityNotification();
    const esp_err_t err = esp_matter::attribute::update(
        s_endpoint, chip::app::Clusters::RelativeHumidityMeasurement::Id,
        chip::app::Clusters::RelativeHumidityMeasurement::Attributes::MeasuredValue::Id, &value);
    if (err != ESP_OK) ESP_LOGE(TAG, "Publish failed: %s", esp_err_to_name(err));
}

esp_err_t read_average(int &raw)
{
    int samples[32];
    for (int &sample : samples) {
        const esp_err_t err = adc_oneshot_read(s_adc, s_channel, &sample);
        if (err != ESP_OK) return err;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    std::sort(samples, samples + 32);
    int sum = 0;
    for (int i = 6; i < 26; ++i) sum += samples[i];
    raw = sum / 20;
    return raw >= 100 && raw <= 4000 ? ESP_OK : ESP_ERR_INVALID_RESPONSE;
}

esp_err_t calibrate(bool dry, int raw)
{
    const int next_dry = dry ? raw : s_dry;
    const int next_wet = dry ? s_wet : raw;
    if (next_dry >= 0 && next_wet >= 0 && next_dry - next_wet < 100) {
        ESP_LOGE(TAG, "Calibration rejected: dry must exceed wet by at least 100 ADC counts");
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = nvs_set_i32(s_nvs, dry ? "dry" : "wet", raw);
    if (err == ESP_OK) err = nvs_commit(s_nvs);
    if (err != ESP_OK) return err;
    s_dry = next_dry;
    s_wet = next_wet;
    ESP_LOGI(TAG, "Saved %s calibration: raw=%d", dry ? "DRY" : "WET", raw);
    return ESP_OK;
}

void worker(void *)
{
    vTaskDelay(pdMS_TO_TICKS(300));
    uint32_t actions = 1;
    for (;;) {
        int raw = 0;
        const esp_err_t err = read_average(raw);
        int value = -1;
        if (err == ESP_OK) {
            const bool dry = (actions & 2) != 0;
            const bool wet = (actions & 4) != 0;
            if (dry != wet) {
                const esp_err_t saved = calibrate(dry, raw);
                if (saved != ESP_OK) ESP_LOGE(TAG, "Calibration not saved: %s", esp_err_to_name(saved));
            } else if (dry) {
                ESP_LOGE(TAG, "Conflicting calibration requests; repeat one at a time");
            }
            value = base::soil::moisture_hundredths(raw, s_dry, s_wet);
            ESP_LOGI(TAG, "ADC GPIO%d raw=%d dry=%ld wet=%ld", CONFIG_BASE_SOIL_ADC_GPIO,
                     raw, static_cast<long>(s_dry), static_cast<long>(s_wet));
            if (value < 0) ESP_LOGW(TAG, "Moisture unknown: calibrate DRY and WET first");
            else ESP_LOGI(TAG, "Relative soil moisture: %d.%02d %%", value / 100, value % 100);
        } else {
            ESP_LOGW(TAG, "ADC reading invalid (%s, raw=%d); publishing unknown", esp_err_to_name(err), raw);
        }
        const CHIP_ERROR scheduled = chip::DeviceLayer::PlatformMgr().ScheduleWork(publish, value);
        if (scheduled != CHIP_NO_ERROR) ESP_LOGE(TAG, "Could not schedule moisture report");
        actions = 1;
        xTaskNotifyWait(0, UINT32_MAX, &actions, pdMS_TO_TICKS(CONFIG_BASE_SOIL_INTERVAL_SECONDS * 1000));
    }
}
}

esp_err_t base::soil::start(uint16_t endpoint_id)
{
    if (s_task != nullptr) return ESP_ERR_INVALID_STATE;
    adc_unit_t unit;
    esp_err_t err = adc_oneshot_io_to_channel(CONFIG_BASE_SOIL_ADC_GPIO, &unit, &s_channel);
    if (err != ESP_OK) return err;
    if (unit != ADC_UNIT_1) return ESP_ERR_INVALID_ARG;
    adc_oneshot_unit_init_cfg_t unit_cfg{};
    unit_cfg.unit_id = unit;
    err = adc_oneshot_new_unit(&unit_cfg, &s_adc);
    if (err != ESP_OK) return err;
    adc_oneshot_chan_cfg_t channel_cfg{};
    channel_cfg.atten = ADC_ATTEN_DB_12;
    channel_cfg.bitwidth = ADC_BITWIDTH_12;
    err = adc_oneshot_config_channel(s_adc, s_channel, &channel_cfg);
    if (err != ESP_OK) return err;
    err = nvs_open("soil_cal", NVS_READWRITE, &s_nvs);
    if (err != ESP_OK) return err;
    err = nvs_get_i32(s_nvs, "dry", &s_dry);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;
    err = nvs_get_i32(s_nvs, "wet", &s_wet);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;
    s_endpoint = endpoint_id;
    if (xTaskCreate(worker, "soil_adc", 4096, nullptr, 4, &s_task) != pdPASS) return ESP_ERR_NO_MEM;
    ESP_LOGI(TAG, "Sensor GPIO%d; VCC=3V3; interval=%d seconds", CONFIG_BASE_SOIL_ADC_GPIO,
             CONFIG_BASE_SOIL_INTERVAL_SECONDS);
    return ESP_OK;
}

esp_err_t base::soil::request(Request action)
{
    if (s_task == nullptr) return ESP_ERR_INVALID_STATE;
    return xTaskNotify(s_task, static_cast<uint32_t>(action), eSetBits) == pdPASS ? ESP_OK : ESP_FAIL;
}
#else
esp_err_t base::soil::start(uint16_t) { return ESP_OK; }
esp_err_t base::soil::request(Request) { return ESP_OK; }
#endif
