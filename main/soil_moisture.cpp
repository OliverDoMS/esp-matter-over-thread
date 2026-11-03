#include "soil_moisture.h"
#include "soil_math.h"
#include "soil_settings.h"
#include "soil_controls.h"

#include <algorithm>
#include <atomic>
#include <driver/gpio.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_log.h>
#include <esp_pm.h>
#include <esp_timer.h>
#include <esp_matter.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <nvs.h>
#include <platform/CHIPDeviceLayer.h>
#include <app/icd/server/ICDNotifier.h>
#include <sdkconfig.h>

#if CONFIG_BASE_SOIL_MOISTURE
namespace {
constexpr char TAG[] = "soil";
TaskHandle_t s_task = nullptr;
QueueHandle_t s_queue = nullptr;
struct Command { base::soil::Request action; uint16_t minutes; bool change_interval; };
esp_pm_lock_handle_t s_measure_lock = nullptr;
adc_channel_t s_channel;
uint16_t s_endpoint;
nvs_handle_t s_nvs;
int32_t s_dry = -1, s_wet = -1;
std::atomic<uint16_t> s_interval_minutes{base::soil::kDefaultIntervalMinutes};
#if CONFIG_BASE_SOIL_POWER_ACTIVE_LOW
constexpr int kPowerOn = 0;
#else
constexpr int kPowerOn = 1;
#endif
#if CONFIG_BASE_SOIL_SWITCHED_POWER
constexpr bool kSwitchedPower = true;
#else
constexpr bool kSwitchedPower = false;
#endif

esp_err_t sensor_power(bool on)
{
#if CONFIG_BASE_SOIL_SWITCHED_POWER
    return gpio_set_level(static_cast<gpio_num_t>(CONFIG_BASE_SOIL_POWER_GPIO), on ? kPowerOn : !kPowerOn);
#else
    return ESP_OK;
#endif
}

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
    // No permanent ADC handle or active PM lock between measurements.
    esp_err_t err = esp_pm_lock_acquire(s_measure_lock);
    if (err != ESP_OK) return err;
    adc_oneshot_unit_handle_t adc = nullptr;
    adc_oneshot_unit_init_cfg_t unit_cfg{};
    unit_cfg.unit_id = ADC_UNIT_1;
    err = adc_oneshot_new_unit(&unit_cfg, &adc);
    adc_oneshot_chan_cfg_t channel_cfg{};
    channel_cfg.atten = ADC_ATTEN_DB_12;
    channel_cfg.bitwidth = ADC_BITWIDTH_12;
    if (err == ESP_OK) err = adc_oneshot_config_channel(adc, s_channel, &channel_cfg);
    if (err == ESP_OK) err = sensor_power(true);
    if (err == ESP_OK) vTaskDelay(pdMS_TO_TICKS(CONFIG_BASE_SOIL_SETTLE_MS));
    int samples[32]{};
    if (err == ESP_OK) {
        for (int &sample : samples) {
            err = adc_oneshot_read(adc, s_channel, &sample);
            if (err != ESP_OK) break;
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
    const esp_err_t off_error = sensor_power(false);
    if (adc != nullptr) adc_oneshot_del_unit(adc);
    esp_pm_lock_release(s_measure_lock);
    if (err != ESP_OK) return err;
    if (off_error != ESP_OK) return off_error;
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
    Command command{base::soil::Request::Measure, 0, false};
    for (;;) {
        if (command.change_interval) {
            esp_err_t err = nvs_set_u16(s_nvs, "interval_min", command.minutes);
            if (err == ESP_OK) err = nvs_commit(s_nvs);
            if (err == ESP_OK) {
                s_interval_minutes.store(command.minutes);
                ESP_LOGI(TAG, "Saved measurement interval: %u minutes", command.minutes);
            } else {
                ESP_LOGE(TAG, "Interval not saved: %s", esp_err_to_name(err));
            }
            base::controls::interval_changed();
        } else {
            int raw = 0;
            const esp_err_t err = read_average(raw);
            bool success = err == ESP_OK;
            int value = -1;
            if (err == ESP_OK) {
                const bool dry = command.action == base::soil::Request::Dry;
                const bool wet = command.action == base::soil::Request::Wet;
                if (dry || wet) {
                    const esp_err_t saved = calibrate(dry, raw);
                    success = saved == ESP_OK;
                    if (saved != ESP_OK) ESP_LOGE(TAG, "Calibration not saved: %s", esp_err_to_name(saved));
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
            base::controls::operation_finished(success && scheduled == CHIP_NO_ERROR);
        }
        // Changing the interval starts a new countdown. Requests wake this wait immediately.
        command = {base::soil::Request::Measure, 0, false};
        xQueueReceive(s_queue, &command, pdMS_TO_TICKS(static_cast<uint32_t>(s_interval_minutes.load()) * 60000U));
    }
}
}

esp_err_t base::soil::load_settings()
{
    esp_err_t err = nvs_open("soil_cal", NVS_READWRITE, &s_nvs);
    if (err != ESP_OK) return err;
    err = nvs_get_i32(s_nvs, "dry", &s_dry);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;
    err = nvs_get_i32(s_nvs, "wet", &s_wet);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;
    uint16_t minutes = kDefaultIntervalMinutes;
    err = nvs_get_u16(s_nvs, "interval_min", &minutes);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;
    if (interval_mode(minutes) < 0) {
        ESP_LOGW(TAG, "Unsupported stored interval; using 60 minutes");
        minutes = kDefaultIntervalMinutes;
    }
    s_interval_minutes.store(minutes);
    return ESP_OK;
}

uint16_t base::soil::interval_minutes() { return s_interval_minutes.load(); }

esp_err_t base::soil::start(uint16_t endpoint_id)
{
    if (s_task != nullptr) return ESP_ERR_INVALID_STATE;
    adc_unit_t unit;
    esp_err_t err = adc_oneshot_io_to_channel(CONFIG_BASE_SOIL_ADC_GPIO, &unit, &s_channel);
    if (err != ESP_OK) return err;
    if (unit != ADC_UNIT_1) return ESP_ERR_INVALID_ARG;
#if CONFIG_BASE_SOIL_SWITCHED_POWER
    static_assert(CONFIG_BASE_SOIL_POWER_GPIO != CONFIG_BASE_SOIL_ADC_GPIO &&
                  CONFIG_BASE_SOIL_POWER_GPIO != CONFIG_BASE_BUTTON_GPIO &&
                  CONFIG_BASE_SOIL_POWER_GPIO != 8 && CONFIG_BASE_SOIL_POWER_GPIO != 13,
                  "Sensor power pin conflicts with ADC, button or LEDs");
    gpio_set_level(static_cast<gpio_num_t>(CONFIG_BASE_SOIL_POWER_GPIO), !kPowerOn);
    gpio_config_t power{};
    power.pin_bit_mask = 1ULL << CONFIG_BASE_SOIL_POWER_GPIO;
    power.mode = GPIO_MODE_OUTPUT;
    err = gpio_config(&power);
    if (err != ESP_OK) return err;
    err = gpio_sleep_sel_dis(static_cast<gpio_num_t>(CONFIG_BASE_SOIL_POWER_GPIO));
    if (err != ESP_OK) return err;
    err = sensor_power(false);
    if (err != ESP_OK) return err;
#endif
    err = esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "soil_measure", &s_measure_lock);
    if (err != ESP_OK) return err;
    s_queue = xQueueCreate(8, sizeof(Command));
    if (s_queue == nullptr) return ESP_ERR_NO_MEM;
    s_endpoint = endpoint_id;
    if (xTaskCreate(worker, "soil_adc", 4096, nullptr, 4, &s_task) != pdPASS) return ESP_ERR_NO_MEM;
    ESP_LOGI(TAG, "Sensor ADC GPIO%d; interval=%u minutes; switched power=%d", CONFIG_BASE_SOIL_ADC_GPIO,
             s_interval_minutes.load(), kSwitchedPower);
    return ESP_OK;
}

esp_err_t base::soil::request(Request action)
{
    if (s_queue == nullptr || s_task == nullptr) return ESP_ERR_INVALID_STATE;
    const Command command{action, 0, false};
    return xQueueSend(s_queue, &command, 0) == pdPASS ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t base::soil::set_interval_minutes(uint16_t minutes)
{
    if (interval_mode(minutes) < 0) return ESP_ERR_INVALID_ARG;
    if (s_queue == nullptr || s_task == nullptr) return ESP_ERR_INVALID_STATE;
    const Command command{Request::Measure, minutes, true};
    return xQueueSend(s_queue, &command, 0) == pdPASS ? ESP_OK : ESP_ERR_TIMEOUT;
}
#else
esp_err_t base::soil::load_settings() { return ESP_OK; }
uint16_t base::soil::interval_minutes() { return 60; }
esp_err_t base::soil::set_interval_minutes(uint16_t) { return ESP_ERR_NOT_SUPPORTED; }
esp_err_t base::soil::start(uint16_t) { return ESP_OK; }
esp_err_t base::soil::request(Request) { return ESP_OK; }
#endif
