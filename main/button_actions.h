#pragma once
#include <cstdint>

namespace base {
enum class ButtonAction : intptr_t { Wake, TestContact, Commission, FactoryReset, CalibrateDry, CalibrateWet };

constexpr ButtonAction action_on_release(int64_t duration_ms, bool soil_sensor = false)
{
    if (duration_ms >= 10000) return ButtonAction::FactoryReset;
    if (soil_sensor && duration_ms >= 6000) return ButtonAction::CalibrateWet;
    if (duration_ms >= 3000) return ButtonAction::Commission;
    if (duration_ms < 1000) return ButtonAction::TestContact;
    if (soil_sensor) return ButtonAction::CalibrateDry;
    return ButtonAction::Wake;
}
}
