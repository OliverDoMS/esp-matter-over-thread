#pragma once
#include <cstdint>

namespace base::soil {
// A relative, two-point soil moisture index; not volumetric water content.
constexpr int moisture_hundredths(int raw, int dry, int wet)
{
    if (raw < 100 || raw > 4000 || wet < 100 || wet > 4000 || dry < 100 || dry > 4000) return -1;
    if (dry - wet < 100) return -1;
    const int value = (dry - raw) * 10000 / (dry - wet);
    return value < 0 ? 0 : value > 10000 ? 10000 : value;
}

// Thresholds from the ESPHome multi-sensor check: 300 mV rise (about 370 of 4095 counts at
// 12 dB) when powered, and at least 25 % of the reading held against the internal pulldown.
inline constexpr int kMinRiseCounts = 370;
constexpr bool sensor_connected(int rest, int raw, int with_pulldown)
{
    return raw - rest >= kMinRiseCounts && with_pulldown * 4 >= raw;
}
}
