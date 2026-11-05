#include "button_actions.h"
#include "soil_math.h"
#include "soil_settings.h"
using base::action_on_release;
using base::ButtonAction;
static_assert(action_on_release(0) == ButtonAction::TestContact);
static_assert(action_on_release(999) == ButtonAction::TestContact);
static_assert(action_on_release(1000) == ButtonAction::Wake);
static_assert(action_on_release(2999) == ButtonAction::Wake);
static_assert(action_on_release(3000) == ButtonAction::Commission);
static_assert(action_on_release(9999) == ButtonAction::Commission);
static_assert(action_on_release(10000) == ButtonAction::FactoryReset);
static_assert(action_on_release(60000) == ButtonAction::FactoryReset);
static_assert(action_on_release(999, true) == ButtonAction::TestContact);
static_assert(action_on_release(1000, true) == ButtonAction::Wake);
static_assert(action_on_release(2000, true) == ButtonAction::Wake);
static_assert(action_on_release(5000, true) == ButtonAction::Wake);
static_assert(action_on_release(9999, true) == ButtonAction::Wake);
static_assert(action_on_release(10000, true) == ButtonAction::FactoryReset);
using base::button_led_on;
static_assert(!button_led_on(1999));
static_assert(!button_led_on(2000));
static_assert(!button_led_on(5000));
static_assert(!button_led_on(9999));
static_assert(button_led_on(10000));
static_assert(!button_led_on(10100));
static_assert(button_led_on(10200));
static_assert(button_led_on(60000));
using base::soil::moisture_hundredths;
static_assert(moisture_hundredths(3000, 3000, 1000) == 0);
static_assert(moisture_hundredths(1000, 3000, 1000) == 10000);
static_assert(moisture_hundredths(2000, 3000, 1000) == 5000);
static_assert(moisture_hundredths(3500, 3000, 1000) == 0);
static_assert(moisture_hundredths(500, 3000, 1000) == 10000);
static_assert(moisture_hundredths(0, 3000, 1000) == -1);
static_assert(moisture_hundredths(4095, 3000, 1000) == -1);
static_assert(moisture_hundredths(2000, -1, -1) == -1);
static_assert(moisture_hundredths(2000, 1000, 3000) == -1);
static_assert(moisture_hundredths(2000, 2100, 2050) == -1);
static_assert(moisture_hundredths(2000, INT32_MAX, INT32_MIN) == -1);
using base::soil::published_hundredths;
static_assert(published_hundredths(5000, -1, -1) == 123); // Not calibrated
static_assert(published_hundredths(-1, -1, -1) == 123); // Not calibrated, not connected
static_assert(published_hundredths(-1, 2752, -1) == 123); // Only dry set
static_assert(published_hundredths(5000, 3000, 1000) == 5000);
static_assert(published_hundredths(-1, 3000, 1000) == -1); // Calibrated, reading failed
static_assert(published_hundredths(0, 3000, 1000) == 0);
using base::soil::sensor_connected;
static_assert(sensor_connected(0, 2752, 2000));
static_assert(sensor_connected(20, 1008, 800));
static_assert(sensor_connected(0, 2752, 688));
static_assert(!sensor_connected(0, 2752, 687)); // Open input collapses under the pulldown
static_assert(!sensor_connected(2000, 2200, 2100)); // No reaction to the supply
static_assert(!sensor_connected(0, 369, 369));
using base::soil::action_is_dry;
using base::soil::action_sensor;
static_assert(base::soil::kActionCount == 2 + 2 * base::soil::kSensorCount);
static_assert(action_sensor(2) == 0 && action_is_dry(2));
static_assert(action_sensor(3) == 0 && !action_is_dry(3));
static_assert(action_sensor(4) == 1 && action_is_dry(4));
static_assert(action_sensor(5) == 1 && !action_is_dry(5));
static_assert(action_sensor(6) == 2 && action_is_dry(6));
static_assert(action_sensor(7) == 2 && !action_is_dry(7));
static_assert(base::soil::kDefaultIntervalMinutes == 60);
static_assert(base::soil::interval_mode(60) == 5);
static_assert(base::soil::interval_mode(1) == 0);
static_assert(base::soil::interval_mode(1440) == 9);
static_assert(base::soil::interval_mode(0) == -1);
static_assert(base::soil::interval_mode(61) == -1);
static_assert(base::soil::interval_mode(65535) == -1);
static_assert(base::soil::interval_wait_ticks(1, 1000) == 60000);
static_assert(base::soil::interval_wait_ticks(60, 1000) == 3600000);
static_assert(base::soil::interval_wait_ticks(120, 1000) == 7200000);
static_assert(base::soil::interval_wait_ticks(1440, 1000) == 86400000);
static_assert(base::soil::interval_wait_ticks(1440, 100) == 8640000);
