#include "board.h"

#include <driver/gpio.h>
#include <esp_check.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <iot_button.h>
#include <button_gpio.h>
#include <led_strip.h>
#include <sdkconfig.h>

namespace {
constexpr char TAG[] = "board";
base::ButtonCallback s_callback = nullptr;
int64_t s_pressed_at = 0;

#if CONFIG_BASE_BUTTON_LED_FEEDBACK
static_assert(CONFIG_BASE_BUTTON_GPIO != 13, "GPIO13 is reserved for the button feedback LED");
base::ButtonAction s_announced_action = base::ButtonAction::Wake;
void held(void *, void *)
{
    const int64_t duration_ms = (esp_timer_get_time() - s_pressed_at) / 1000;
    gpio_set_level(GPIO_NUM_13, base::button_led_on(duration_ms) ? 1 : 0);
    const auto action = base::action_on_release(duration_ms, true);
    if (action != s_announced_action && action == base::ButtonAction::FactoryReset) {
        s_announced_action = action;
        ESP_LOGI(TAG, "Release BOOT for MATTER FACTORY RESET (rapid blinking)");
    }
}
#endif

#if CONFIG_BASE_SOIL_MOISTURE
void single_click(void *, void *) { s_callback(base::ButtonAction::TestContact); }
void double_click(void *, void *) { s_callback(base::ButtonAction::Commission); }
#endif

void pressed(void *, void *)
{
    s_pressed_at = esp_timer_get_time();
#if CONFIG_BASE_BUTTON_LED_FEEDBACK
    s_announced_action = base::ButtonAction::Wake;
    gpio_set_level(GPIO_NUM_13, 0);
#endif
    s_callback(base::ButtonAction::Wake);
}

void released(void *, void *)
{
    const int64_t duration_ms = (esp_timer_get_time() - s_pressed_at) / 1000;
    // Actions execute only on release, so a reset hold does not first open pairing.
#if CONFIG_BASE_SOIL_MOISTURE
#if CONFIG_BASE_BUTTON_LED_FEEDBACK
    gpio_set_level(GPIO_NUM_13, 0);
#endif
    const auto action = base::action_on_release(duration_ms, true);
    ESP_LOGI(TAG, "BOOT released after %lld ms; action=%d", static_cast<long long>(duration_ms), static_cast<int>(action));
    // Short presses wait for SINGLE/DOUBLE_CLICK to avoid measuring twice on pairing.
    if (action != base::ButtonAction::TestContact) s_callback(action);
#else
    s_callback(base::action_on_release(duration_ms));
#endif
}

esp_err_t disable_leds()
{
#if CONFIG_BASE_DISABLE_SUPERMINI_LEDS
    gpio_config_t blue{};
    blue.pin_bit_mask = 1ULL << GPIO_NUM_13;
    blue.mode = GPIO_MODE_OUTPUT;
    ESP_RETURN_ON_ERROR(gpio_config(&blue), TAG, "Blue LED GPIO failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(GPIO_NUM_13, 0), TAG, "Blue LED off failed");

    led_strip_config_t strip{};
    strip.strip_gpio_num = GPIO_NUM_8;
    strip.max_leds = 1;
    strip.led_pixel_format = LED_PIXEL_FORMAT_GRB;
    strip.led_model = LED_MODEL_WS2812;
    led_strip_rmt_config_t rmt{};
    rmt.clk_src = RMT_CLK_SRC_DEFAULT;
    rmt.resolution_hz = 10000000;
    led_strip_handle_t handle = nullptr;
    ESP_RETURN_ON_ERROR(led_strip_new_rmt_device(&strip, &rmt, &handle), TAG, "RGB init failed");
    const esp_err_t clear_err = led_strip_clear(handle);
    const esp_err_t delete_err = led_strip_del(handle);
    ESP_RETURN_ON_ERROR(clear_err, TAG, "RGB clear failed");
    ESP_RETURN_ON_ERROR(delete_err, TAG, "RGB driver release failed");
    // Release RMT resources immediately; keep only a static low data signal.
    ESP_RETURN_ON_ERROR(gpio_set_direction(GPIO_NUM_8, GPIO_MODE_OUTPUT), TAG, "RGB GPIO failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(GPIO_NUM_8, 0), TAG, "RGB GPIO low failed");
#endif
    return ESP_OK;
}
}

esp_err_t base::init_board(ButtonCallback callback)
{
    if (callback == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    s_callback = callback;
    ESP_RETURN_ON_ERROR(disable_leds(), TAG, "LED initialization failed");
#if CONFIG_BASE_BUTTON_LED_FEEDBACK
    ESP_RETURN_ON_ERROR(gpio_sleep_sel_dis(GPIO_NUM_13), TAG, "Keep LED state during Light-sleep failed");
#endif
    button_config_t button_config{};
#if CONFIG_BASE_SOIL_MOISTURE
    button_config.long_press_time = 1000; // Begin feedback callbacks well before the reset cue at 10 s.
    button_config.short_press_time = 400; // Comfortable double-click interval.
#endif
    button_gpio_config_t gpio_config{};
    gpio_config.gpio_num = CONFIG_BASE_BUTTON_GPIO;
    gpio_config.active_level = 0;
    gpio_config.enable_power_save = true;
    // Keep peripheral power-down disabled in sdkconfig: button 4.2.3 disables
    // EXT1 on wake but only re-arms GPIO wake on the next power-save entry.
    button_handle_t button = nullptr;
    ESP_RETURN_ON_ERROR(iot_button_new_gpio_device(&button_config, &gpio_config, &button), TAG, "Button init failed");
    ESP_RETURN_ON_ERROR(iot_button_register_cb(button, BUTTON_PRESS_DOWN, nullptr, pressed, nullptr), TAG, "Button down callback failed");
    ESP_RETURN_ON_ERROR(iot_button_register_cb(button, BUTTON_PRESS_UP, nullptr, released, nullptr), TAG, "Button up callback failed");
#if CONFIG_BASE_SOIL_MOISTURE
    ESP_RETURN_ON_ERROR(iot_button_register_cb(button, BUTTON_SINGLE_CLICK, nullptr, single_click, nullptr), TAG, "Single click callback failed");
    ESP_RETURN_ON_ERROR(iot_button_register_cb(button, BUTTON_DOUBLE_CLICK, nullptr, double_click, nullptr), TAG, "Double click callback failed");
#endif
#if CONFIG_BASE_BUTTON_LED_FEEDBACK
    ESP_RETURN_ON_ERROR(iot_button_register_cb(button, BUTTON_LONG_PRESS_HOLD, nullptr, held, nullptr), TAG, "LED feedback callback failed");
    ESP_LOGI(TAG, "Blue GPIO13 LED: rapid blinking RESET; release to confirm");
#endif
    ESP_LOGI(TAG, "BOOT GPIO%d: power-save button with GPIO wakeup; peripheral power-down disabled",
             CONFIG_BASE_BUTTON_GPIO);
    return ESP_OK;
}
