#pragma once
#include <cstdint>

namespace base::soil {
inline constexpr uint8_t kSensorCount = 3;
inline constexpr uint8_t kAllSensors = 0xFF;

// Modes 0 and 1 are idle and measure; from mode 2 on, pairs of dry and wet per sensor.
inline constexpr const char *kActionLabels[] = {
    "Bereit", "Jetzt messen",
    "Sensor 1 trocken", "Sensor 1 nass",
    "Sensor 2 trocken", "Sensor 2 nass",
    "Sensor 3 trocken", "Sensor 3 nass",
};
inline constexpr uint8_t kActionCount = sizeof(kActionLabels) / sizeof(kActionLabels[0]);
inline constexpr uint8_t kMeasureMode = 1;
constexpr uint8_t action_sensor(uint8_t mode) { return static_cast<uint8_t>((mode - 2) / 2); }
constexpr bool action_is_dry(uint8_t mode) { return mode % 2 == 0; }

struct IntervalOption { uint16_t minutes; const char *label; };
inline constexpr IntervalOption kIntervals[] = {
    {1, "1 Minute"}, {5, "5 Minuten"}, {10, "10 Minuten"}, {15, "15 Minuten"},
    {30, "30 Minuten"}, {60, "60 Minuten"}, {120, "120 Minuten"},
    {360, "360 Minuten"}, {720, "720 Minuten"}, {1440, "1440 Minuten"}
};
inline constexpr uint16_t kDefaultIntervalMinutes = 60;
inline constexpr uint8_t kIntervalCount = sizeof(kIntervals) / sizeof(kIntervals[0]);
constexpr int interval_mode(uint16_t minutes)
{
    for (uint8_t i = 0; i < kIntervalCount; ++i) if (kIntervals[i].minutes == minutes) return i;
    return -1;
}
constexpr uint32_t interval_wait_ticks(uint16_t minutes, uint32_t tick_hz)
{
    // Convert minutes directly with a wide intermediate; long intervals must not wrap.
    return static_cast<uint32_t>(static_cast<uint64_t>(minutes) * 60U * tick_hz);
}
}
