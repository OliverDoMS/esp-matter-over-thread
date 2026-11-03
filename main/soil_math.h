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
}
