#include "soil_moisture.h"
#include "soil_math.h"
#include "soil_settings.h"
#include "soil_controls.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
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
using base::soil::kAllSensors;
using base::soil::kSensorCount;
TaskHandle_t s_task = nullptr;
QueueHandle_t s_queue = nullptr;
struct Command { base::soil::Request action; uint8_t sensor; uint16_t minutes; bool change_interval; };
esp_pm_lock_handle_t s_measure_lock = nullptr;
nvs_handle_t s_nvs;
std::atomic<uint16_t> s_interval_minutes{base::soil::kDefaultIntervalMinutes};

struct Pins { int adc; int power; };
constexpr Pins kPins[kSensorCount] = {
    {CONFIG_BASE_SOIL_1_ADC_GPIO, CONFIG_BASE_SOIL_1_POWER_GPIO},
    {CONFIG_BASE_SOIL_2_ADC_GPIO, CONFIG_BASE_SOIL_2_POWER_GPIO},
    {CONFIG_BASE_SOIL_3_ADC_GPIO, CONFIG_BASE_SOIL_3_POWER_GPIO},
};

// Each sensor keeps its own references; a rejected calibration touches only that sensor.
struct Sensor {
    adc_channel_t channel{};
    uint16_t endpoint = 0;
    int32_t dry = -1, wet = -1;
    std::atomic<int> value{-1};
};
Sensor s_sensors[kSensorCount];

#if CONFIG_BASE_SOIL_CONNECTION_CHECK
constexpr bool kConnectionCheck = true;
#else
constexpr bool kConnectionCheck = false;
#endif

constexpr bool pins_valid()
{
    for (int i = 0; i < kSensorCount; ++i) {
        const int power = kPins[i].power;
        if (power == CONFIG_BASE_BUTTON_GPIO || power == 8 || power == 13) return false;
        for (int j = 0; j < kSensorCount; ++j) {
            if (power == kPins[j].adc) return false;
            if (i != j && (power == kPins[j].power || kPins[i].adc == kPins[j].adc)) return false;
        }
    }
    return true;
}
static_assert(pins_valid(), "Sensor pins conflict with each other, the button or the LEDs");

esp_err_t sensor_power(uint8_t sensor, bool on)
{
    return gpio_set_level(static_cast<gpio_num_t>(kPins[sensor].power), on ? 1 : 0);
}

esp_err_t adc_pulldown(uint8_t sensor, bool on)
{
    return gpio_set_pull_mode(static_cast<gpio_num_t>(kPins[sensor].adc), on ? GPIO_PULLDOWN_ONLY : GPIO_FLOATING);
}

void publish(intptr_t sensor)
{
    // Scheduled onto the Matter loop; never publish directly from the ADC task.
    const Sensor &target = s_sensors[sensor];
    const int reading = target.value.load();
    nullable<uint16_t> measured;
    if (reading >= 0) measured = static_cast<uint16_t>(reading);
    auto value = esp_matter_nullable_uint16(measured);
    chip::app::ICDNotifier::GetInstance().NotifyNetworkActivityNotification();
    const esp_err_t err = esp_matter::attribute::update(
        target.endpoint, chip::app::Clusters::RelativeHumidityMeasurement::Id,
        chip::app::Clusters::RelativeHumidityMeasurement::Attributes::MeasuredValue::Id, &value);
    if (err != ESP_OK) ESP_LOGE(TAG, "Sensor %d: publish failed: %s", static_cast<int>(sensor) + 1, esp_err_to_name(err));
}

esp_err_t read_mean(adc_oneshot_unit_handle_t adc, adc_channel_t channel, int &mean)
{
    constexpr int kCount = 3;
    int sum = 0;
    for (int i = 0; i < kCount; ++i) {
        int sample = 0;
        const esp_err_t err = adc_oneshot_read(adc, channel, &sample);
        if (err != ESP_OK) return err;
        sum += sample;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    mean = sum / kCount;
    return ESP_OK;
}

// A connected sensor rests near 0 V and holds its output against the pulldown.
// An open input follows neither; without this check it could read as a plausible value.
esp_err_t read_with_pulldown(adc_oneshot_unit_handle_t adc, uint8_t sensor, uint32_t discharge_ms, int &mean)
{
    esp_err_t err = adc_pulldown(sensor, true);
    if (err == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(discharge_ms));
        err = read_mean(adc, s_sensors[sensor].channel, mean);
    }
    const esp_err_t released = adc_pulldown(sensor, false);
    return err != ESP_OK ? err : released;
}

// Only this sensor is powered while it is read. The caller holds ADC unit and PM lock.
esp_err_t read_sensor(adc_oneshot_unit_handle_t adc, uint8_t sensor, int &raw)
{
    const adc_channel_t channel = s_sensors[sensor].channel;
    adc_oneshot_chan_cfg_t channel_cfg{};
    channel_cfg.atten = ADC_ATTEN_DB_12;
    channel_cfg.bitwidth = ADC_BITWIDTH_12;
    esp_err_t err = adc_oneshot_config_channel(adc, channel, &channel_cfg);
    int rest = 0, loaded = 0;
    if (err == ESP_OK && kConnectionCheck) err = read_with_pulldown(adc, sensor, 60, rest);
    if (err == ESP_OK) err = sensor_power(sensor, true);
    if (err == ESP_OK) vTaskDelay(pdMS_TO_TICKS(CONFIG_BASE_SOIL_SETTLE_MS));
    int samples[32]{};
    if (err == ESP_OK) {
        for (int &sample : samples) {
            err = adc_oneshot_read(adc, channel, &sample);
            if (err != ESP_OK) break;
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
    if (err == ESP_OK && kConnectionCheck) err = read_with_pulldown(adc, sensor, 30, loaded);
    const esp_err_t off_error = sensor_power(sensor, false);
    if (err != ESP_OK) return err;
    if (off_error != ESP_OK) return off_error;
    std::sort(samples, samples + 32);
    int sum = 0;
    for (int i = 6; i < 26; ++i) sum += samples[i];
    raw = sum / 20;
    if (kConnectionCheck && !base::soil::sensor_connected(rest, raw, loaded)) {
        ESP_LOGW(TAG, "Sensor %d not connected? rest=%d raw=%d with pulldown=%d", sensor + 1, rest, raw, loaded);
        return ESP_ERR_NOT_FOUND;
    }
    return raw >= 100 && raw <= 4000 ? ESP_OK : ESP_ERR_INVALID_RESPONSE;
}

void calibration_key(char (&key)[8], bool dry, uint8_t sensor)
{
    std::snprintf(key, sizeof(key), "%s%d", dry ? "dry" : "wet", sensor + 1);
}

esp_err_t calibrate(uint8_t sensor, bool dry, int raw)
{
    Sensor &target = s_sensors[sensor];
    const int next_dry = dry ? raw : target.dry;
    const int next_wet = dry ? target.wet : raw;
    if (next_dry >= 0 && next_wet >= 0 && next_dry - next_wet < 100) {
        ESP_LOGE(TAG, "Sensor %d: calibration rejected, dry must exceed wet by at least 100 ADC counts", sensor + 1);
        return ESP_ERR_INVALID_ARG;
    }
    char key[8];
    calibration_key(key, dry, sensor);
    esp_err_t err = nvs_set_i32(s_nvs, key, raw);
    if (err == ESP_OK) err = nvs_commit(s_nvs);
    if (err != ESP_OK) return err;
    target.dry = next_dry;
    target.wet = next_wet;
    ESP_LOGI(TAG, "Sensor %d: saved %s calibration, raw=%d", sensor + 1, dry ? "DRY" : "WET", raw);
    return ESP_OK;
}

void measure(const Command &command)
{
    const bool dry = command.action == base::soil::Request::Dry;
    const bool wet = command.action == base::soil::Request::Wet;
    const uint8_t first = command.sensor == kAllSensors ? 0 : command.sensor;
    const uint8_t end = command.sensor == kAllSensors ? kSensorCount : command.sensor + 1;
    // No permanent ADC handle or active PM lock between measurements.
    esp_err_t setup = esp_pm_lock_acquire(s_measure_lock);
    const bool locked = setup == ESP_OK;
    adc_oneshot_unit_handle_t adc = nullptr;
    if (setup == ESP_OK) {
        adc_oneshot_unit_init_cfg_t unit_cfg{};
        unit_cfg.unit_id = ADC_UNIT_1;
        setup = adc_oneshot_new_unit(&unit_cfg, &adc);
    }
    bool success = true;
    for (uint8_t i = first; i < end; ++i) {
        Sensor &target = s_sensors[i];
        int raw = 0;
        const esp_err_t err = setup == ESP_OK ? read_sensor(adc, i, raw) : setup;
        int value = -1;
        if (err == ESP_OK) {
            if (dry || wet) {
                const esp_err_t saved = calibrate(i, dry, raw);
                if (saved != ESP_OK) {
                    success = false;
                    ESP_LOGE(TAG, "Sensor %d: calibration not saved: %s", i + 1, esp_err_to_name(saved));
                }
            }
            value = base::soil::moisture_hundredths(raw, target.dry, target.wet);
            ESP_LOGI(TAG, "Sensor %d: ADC GPIO%d raw=%d dry=%ld wet=%ld", i + 1, kPins[i].adc, raw,
                     static_cast<long>(target.dry), static_cast<long>(target.wet));
            if (value < 0) ESP_LOGW(TAG, "Sensor %d: moisture unknown, calibrate DRY and WET first", i + 1);
            else ESP_LOGI(TAG, "Sensor %d: relative soil moisture %d.%02d %%", i + 1, value / 100, value % 100);
        } else {
            success = false;
            ESP_LOGW(TAG, "Sensor %d: reading invalid (%s, raw=%d); publishing unknown", i + 1, esp_err_to_name(err), raw);
        }
        target.value.store(value);
        if (chip::DeviceLayer::PlatformMgr().ScheduleWork(publish, i) != CHIP_NO_ERROR) {
            success = false;
            ESP_LOGE(TAG, "Sensor %d: could not schedule moisture report", i + 1);
        }
    }
    if (adc != nullptr) adc_oneshot_del_unit(adc);
    if (locked) esp_pm_lock_release(s_measure_lock);
    base::controls::operation_finished(success);
}

void worker(void *)
{
    Command command{base::soil::Request::Measure, kAllSensors, 0, false};
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
            measure(command);
        }
        // Changing the interval starts a new countdown. Requests wake this wait immediately.
        command = {base::soil::Request::Measure, kAllSensors, 0, false};
        xQueueReceive(s_queue, &command,
                      static_cast<TickType_t>(base::soil::interval_wait_ticks(s_interval_minutes.load(), configTICK_RATE_HZ)));
    }
}
}

esp_err_t base::soil::load_settings()
{
    esp_err_t err = nvs_open("soil_cal", NVS_READWRITE, &s_nvs);
    if (err != ESP_OK) return err;
    for (uint8_t i = 0; i < kSensorCount; ++i) {
        char key[8];
        calibration_key(key, true, i);
        err = nvs_get_i32(s_nvs, key, &s_sensors[i].dry);
        if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;
        calibration_key(key, false, i);
        err = nvs_get_i32(s_nvs, key, &s_sensors[i].wet);
        if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;
    }
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

esp_err_t base::soil::start(const uint16_t *endpoint_ids)
{
    if (s_task != nullptr) return ESP_ERR_INVALID_STATE;
    for (uint8_t i = 0; i < kSensorCount; ++i) {
        adc_unit_t unit;
        esp_err_t err = adc_oneshot_io_to_channel(kPins[i].adc, &unit, &s_sensors[i].channel);
        if (err != ESP_OK) return err;
        if (unit != ADC_UNIT_1) return ESP_ERR_INVALID_ARG;
        const auto power_gpio = static_cast<gpio_num_t>(kPins[i].power);
        gpio_set_level(power_gpio, 0);
        gpio_config_t power{};
        power.pin_bit_mask = 1ULL << kPins[i].power;
        power.mode = GPIO_MODE_OUTPUT;
        err = gpio_config(&power);
        if (err != ESP_OK) return err;
        err = gpio_set_drive_capability(power_gpio, GPIO_DRIVE_CAP_3);
        if (err != ESP_OK) return err;
        err = gpio_sleep_sel_dis(power_gpio);
        if (err != ESP_OK) return err;
        err = sensor_power(i, false);
        if (err != ESP_OK) return err;
        s_sensors[i].endpoint = endpoint_ids[i];
        ESP_LOGI(TAG, "Sensor %d: AOUT GPIO%d, VCC GPIO%d, endpoint %u", i + 1, kPins[i].adc, kPins[i].power, endpoint_ids[i]);
    }
    esp_err_t err = esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "soil_measure", &s_measure_lock);
    if (err != ESP_OK) return err;
    s_queue = xQueueCreate(8, sizeof(Command));
    if (s_queue == nullptr) return ESP_ERR_NO_MEM;
    if (xTaskCreate(worker, "soil_adc", 4096, nullptr, 4, &s_task) != pdPASS) return ESP_ERR_NO_MEM;
    ESP_LOGI(TAG, "%d sensors; interval=%u minutes; connection check=%d", kSensorCount,
             s_interval_minutes.load(), kConnectionCheck);
    return ESP_OK;
}

esp_err_t base::soil::request(Request action, uint8_t sensor)
{
    if (s_queue == nullptr || s_task == nullptr) return ESP_ERR_INVALID_STATE;
    if (action == Request::Measure ? sensor != kAllSensors : sensor >= kSensorCount) return ESP_ERR_INVALID_ARG;
    const Command command{action, sensor, 0, false};
    return xQueueSend(s_queue, &command, 0) == pdPASS ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t base::soil::set_interval_minutes(uint16_t minutes)
{
    if (interval_mode(minutes) < 0) return ESP_ERR_INVALID_ARG;
    if (s_queue == nullptr || s_task == nullptr) return ESP_ERR_INVALID_STATE;
    const Command command{Request::Measure, kAllSensors, minutes, true};
    return xQueueSend(s_queue, &command, 0) == pdPASS ? ESP_OK : ESP_ERR_TIMEOUT;
}
#else
esp_err_t base::soil::load_settings() { return ESP_OK; }
uint16_t base::soil::interval_minutes() { return 60; }
esp_err_t base::soil::set_interval_minutes(uint16_t) { return ESP_ERR_NOT_SUPPORTED; }
esp_err_t base::soil::start(const uint16_t *) { return ESP_OK; }
esp_err_t base::soil::request(Request, uint8_t) { return ESP_OK; }
#endif
