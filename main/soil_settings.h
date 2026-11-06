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

// After an action the same selection shows its outcome. These modes only report;
// choosing one in Home Assistant does nothing.
enum class Result : uint8_t {
    Ok, NotConnected1, NotConnected2, NotConnected3, DryNotAboveWet, Invalid, Busy
};
inline constexpr const char *kResultLabels[] = {
    "Ergebnis: OK",
    "Fehler: Sensor 1 nicht erkannt", "Fehler: Sensor 2 nicht erkannt", "Fehler: Sensor 3 nicht erkannt",
    "Fehler: trocken muss über nass liegen",
    "Fehler: Messwert ungültig",
    "Fehler: Gerät beschäftigt",
};
inline constexpr uint8_t kResultCount = sizeof(kResultLabels) / sizeof(kResultLabels[0]);
inline constexpr uint8_t kModeCount = kActionCount + kResultCount;
static_assert(kResultCount == static_cast<uint8_t>(Result::Busy) + 1, "One label per result");
constexpr uint8_t result_mode(Result result) { return static_cast<uint8_t>(kActionCount + static_cast<uint8_t>(result)); }
constexpr Result not_connected(uint8_t sensor)
{
    return static_cast<Result>(static_cast<uint8_t>(Result::NotConnected1) + sensor);
}
constexpr const char *mode_label(uint8_t mode)
{
    return mode < kActionCount ? kActionLabels[mode] : mode < kModeCount ? kResultLabels[mode - kActionCount] : "";
}
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
// Shared thresholds for all sensors, as in the ESPHome soil sensor. "Watering needed below"
// drives the per-sensor "soil moist" binary sensor; "very wet from" is only offered to Home
// Assistant (template sensors), the device does not act on it.
struct ChoiceOption { uint8_t value; const char *label; };
inline constexpr ChoiceOption kWaterBelow[] = {
    {5, "5 %"}, {10, "10 %"}, {15, "15 %"}, {20, "20 %"}, {25, "25 %"}, {30, "30 %"}, {40, "40 %"}
};
inline constexpr ChoiceOption kWetAbove[] = {
    {50, "50 %"}, {60, "60 %"}, {70, "70 %"}, {75, "75 %"}, {80, "80 %"}, {90, "90 %"}
};
// ADC readings per measurement; a trimmed mean drops the outer 3/16 on each side.
inline constexpr ChoiceOption kSamples[] = {
    {4, "4"}, {8, "8"}, {16, "16"}, {32, "32"}, {64, "64"}
};
inline constexpr uint8_t kWaterBelowCount = sizeof(kWaterBelow) / sizeof(kWaterBelow[0]);
inline constexpr uint8_t kWetAboveCount = sizeof(kWetAbove) / sizeof(kWetAbove[0]);
inline constexpr uint8_t kSamplesCount = sizeof(kSamples) / sizeof(kSamples[0]);
inline constexpr uint8_t kDefaultWaterBelow = 10, kDefaultWetAbove = 75, kDefaultSamples = 32;
inline constexpr uint8_t kMaxSamples = 64;
template <unsigned N>
constexpr int choice_mode(const ChoiceOption (&options)[N], uint8_t value)
{
    for (unsigned i = 0; i < N; ++i) if (options[i].value == value) return static_cast<int>(i);
    return -1;
}
constexpr uint8_t trimmed_per_side(uint8_t samples) { return static_cast<uint8_t>(samples * 3 / 16); }
static_assert(kWaterBelow[kWaterBelowCount - 1].value < kWetAbove[0].value, "Thresholds must not overlap");

// Settings changed through the ADC worker so NVS writes never run on the Matter thread.
enum class Setting : uint8_t { None, Interval, WaterBelow, WetAbove, Samples };

constexpr uint32_t interval_wait_ticks(uint16_t minutes, uint32_t tick_hz)
{
    // Convert minutes directly with a wide intermediate; long intervals must not wrap.
    return static_cast<uint32_t>(static_cast<uint64_t>(minutes) * 60U * tick_hz);
}
}
