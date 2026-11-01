// Based on Espressif's public-domain icd_app; application additions Apache-2.0.
#include "board.h"
#include "sensors.h"

#include <cstdio>
#include <cstring>
#include <esp_check.h>
#include <esp_log.h>
#include <esp_pm.h>
#include <esp_system.h>
#include <esp_app_desc.h>
#include <nvs_flash.h>
#include <esp_matter.h>
#include <app/icd/server/ICDNotifier.h>
#include <app/server/CommissioningWindowManager.h>
#include <app/server/Server.h>
#include <platform/CHIPDeviceLayer.h>
#include <platform/ESP32/OpenthreadLauncher.h>

#if !CONFIG_IDF_TARGET_ESP32H2 || !CONFIG_ENABLE_ICD_SERVER || !CONFIG_OPENTHREAD_MTD
#error "This base profile requires ESP32-H2, OpenThread MTD and the Matter ICD server"
#endif

namespace {
constexpr char TAG[] = "base";
#if CONFIG_BASE_DIAGNOSTIC_NO_SLEEP
constexpr bool kDiagnosticNoSleep = true;
#else
constexpr bool kDiagnosticNoSleep = false;
#endif

void open_commissioning_window()
{
    auto &manager = chip::Server::GetInstance().GetCommissioningWindowManager();
    if (manager.IsCommissioningWindowOpen()) {
        ESP_LOGI(TAG, "Commissioning window already open");
        return;
    }
    const CHIP_ERROR err = manager.OpenBasicCommissioningWindow(chip::System::Clock::Seconds16(300));
    if (err != CHIP_NO_ERROR) {
        ESP_LOGE(TAG, "Could not open commissioning window: %" CHIP_ERROR_FORMAT, err.Format());
    } else {
        ESP_LOGI(TAG, "Commissioning window opened for 300 seconds");
    }
}

void handle_button(intptr_t action_value)
{
    // ScheduleWork executes this function with the Matter stack lock held.
    chip::app::ICDNotifier::GetInstance().NotifyNetworkActivityNotification();
    switch (static_cast<base::ButtonAction>(action_value)) {
    case base::ButtonAction::Wake:
        break;
    case base::ButtonAction::TestContact:
        ESP_ERROR_CHECK_WITHOUT_ABORT(base::sensors::toggle_test_contact());
        break;
    case base::ButtonAction::Commission:
        open_commissioning_window();
        break;
    case base::ButtonAction::FactoryReset:
        ESP_LOGW(TAG, "Factory reset requested by 10-second button hold; removing fabrics and network settings");
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_matter::factory_reset());
        break;
    }
}

void queue_button(base::ButtonAction action)
{
    const CHIP_ERROR err = chip::DeviceLayer::PlatformMgr().ScheduleWork(handle_button, static_cast<intptr_t>(action));
    if (err != CHIP_NO_ERROR) {
        ESP_LOGE(TAG, "Could not schedule button action: %" CHIP_ERROR_FORMAT, err.Format());
    }
}

void device_event(const ChipDeviceEvent *event, intptr_t)
{
    using namespace chip::DeviceLayer;
    switch (event->Type) {
    case DeviceEventType::kCommissioningComplete:
        ESP_LOGI(TAG, "Commissioning complete; automatic Light-sleep %s; ICD enabled",
                 kDiagnosticNoSleep ? "disabled (diagnostic)" : "enabled");
        break;
    case DeviceEventType::kCommissioningWindowOpened:
        ESP_LOGI(TAG, "Commissioning window open");
        break;
    case DeviceEventType::kCommissioningWindowClosed:
        ESP_LOGI(TAG, "Commissioning window closed");
        break;
    case DeviceEventType::kFailSafeTimerExpired:
        ESP_LOGW(TAG, "Commissioning failed or timed out; use BOOT 3-9 s to retry");
        break;
    case DeviceEventType::kInterfaceIpAddressChanged:
        ESP_LOGI(TAG, "IPv6 interface address changed");
        break;
    case DeviceEventType::kFabricRemoved:
        if (chip::Server::GetInstance().GetFabricTable().FabricCount() == 0) {
            open_commissioning_window();
        }
        break;
    default:
        break;
    }
}

esp_err_t attribute_changed(esp_matter::attribute::callback_type_t, uint16_t, uint32_t,
                            uint32_t, esp_matter_attr_val_t *, void *)
{
    return ESP_OK;
}

esp_err_t identify(esp_matter::identification::callback_type_t, uint16_t endpoint_id,
                   uint8_t, uint8_t, void *)
{
    ESP_LOGI(TAG, "Identify requested for endpoint %u (no indicator in low-power base)", endpoint_id);
    return ESP_OK;
}
}

extern "C" void app_main()
{
    ESP_LOGI(TAG, "Firmware %s; reset reason=%d", esp_app_get_description()->version,
             static_cast<int>(esp_reset_reason()));
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS cannot be opened; erasing NVS to recover");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    esp_pm_config_t pm{};
    pm.max_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ;
    pm.min_freq_mhz = 32;
    pm.light_sleep_enable = !kDiagnosticNoSleep;
    ESP_ERROR_CHECK(esp_pm_configure(&pm));
    if (kDiagnosticNoSleep) {
        ESP_LOGW(TAG, "DIAGNOSTIC: automatic Light-sleep DISABLED; not for battery measurements");
    } else {
        ESP_LOGI(TAG, "LOW POWER: automatic Light-sleep ENABLED; USB host may inhibit sleep");
    }

    esp_matter::node::config_t config{};
    static_assert(sizeof(CONFIG_BASE_NODE_LABEL) <= sizeof(config.root_node.basic_information.node_label),
                  "BASE_NODE_LABEL exceeds Matter's maximum node label length");
    std::snprintf(config.root_node.basic_information.node_label,
                  sizeof(config.root_node.basic_information.node_label), "%s", CONFIG_BASE_NODE_LABEL);
    auto *node = esp_matter::node::create(&config, attribute_changed, identify);
    ESP_ERROR_CHECK(node != nullptr ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(base::sensors::create_endpoints(node));

    esp_openthread_platform_config_t thread{};
    thread.radio_config.radio_mode = RADIO_MODE_NATIVE;
    thread.host_config.host_connection_mode = HOST_CONNECTION_MODE_NONE;
    thread.port_config.storage_partition_name = "nvs";
    thread.port_config.netif_queue_size = 10;
    thread.port_config.task_queue_size = 10;
    ESP_ERROR_CHECK(set_openthread_platform_config(&thread));

    ESP_ERROR_CHECK(esp_matter::start(device_event));
    // Initialize the button only after ScheduleWork can safely reach the running stack.
    ESP_ERROR_CHECK(base::init_board(queue_button));
    ESP_LOGI(TAG, "DoNetIoT H2 Base %s: Thread sleepy end device / SIT ICD",
             esp_app_get_description()->version);
    ESP_LOGI(TAG, "BOOT: <1 s TEST contact; 3-9 s pairing; >=10 s factory reset, on release");
    ESP_LOGI(TAG, "Battery measurement not fitted/enabled; no invented voltage or percentage");
}
