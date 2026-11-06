#pragma once
#include <cstdint>

namespace base::soil {
constexpr bool references_valid(int dry, int wet)
{
    return wet >= 100 && wet <= 4000 && dry >= 100 && dry <= 4000 && dry - wet >= 100;
}

// A relative, two-point soil moisture index; not volumetric water content.
constexpr int moisture_hundredths(int raw, int dry, int wet)
{
    if (raw < 100 || raw > 4000 || !references_valid(dry, wet)) return -1;
    const int value = (dry - raw) * 10000 / (dry - wet);
    return value < 0 ? 0 : value > 10000 ? 10000 : value;
}

constexpr bool reference_set(int reference) { return reference >= 100 && reference <= 4000; }

// Placeholders for a sensor without both references, so Home Assistant shows the
// calibration progress: 1.23 % nothing set, 2.34 % only DRY set, 3.45 % only WET set.
// Home Assistant skips endpoints whose value is null at its interview, so null would hide
// the sensor. A real reading can coincide with these values only by chance.
inline constexpr int kUncalibratedHundredths = 123;
inline constexpr int kOnlyDryHundredths = 234;
inline constexpr int kOnlyWetHundredths = 345;
constexpr int placeholder_hundredths(int dry, int wet)
{
    if (references_valid(dry, wet)) return -1;
    if (reference_set(dry) && !reference_set(wet)) return kOnlyDryHundredths;
    if (reference_set(wet) && !reference_set(dry)) return kOnlyWetHundredths;
    return kUncalibratedHundredths;
}
// -1 means unknown (null); only a calibrated sensor can be unknown.
constexpr int published_hundredths(int measured, int dry, int wet)
{
    return references_valid(dry, wet) ? measured : placeholder_hundredths(dry, wet);
}

// "Soil moist" binary sensor: true unless a calibrated, valid reading is below the
// watering threshold. Uncalibrated or failed readings never raise a watering alarm.
constexpr bool moist_enough(int hundredths, int dry, int wet, int water_below_percent)
{
    if (!references_valid(dry, wet) || hundredths < 0) return true;
    return hundredths >= water_below_percent * 100;
}

// Thresholds from the ESPHome multi-sensor check: 300 mV rise (about 370 of 4095 counts at
// 12 dB) when powered, and at least 25 % of the reading held against the internal pulldown.
inline constexpr int kMinRiseCounts = 370;
constexpr bool sensor_connected(int rest, int raw, int with_pulldown)
{
    return raw - rest >= kMinRiseCounts && with_pulldown * 4 >= raw;
}
}
