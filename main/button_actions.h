#pragma once
#include <cstdint>

namespace base {
enum class ButtonAction : intptr_t { Wake, TestContact, Commission, FactoryReset, CalibrateDry, CalibrateWet };

constexpr ButtonAction action_on_release(int64_t duration_ms, bool soil_sensor = false)
{
    if (duration_ms >= 10000) return ButtonAction::FactoryReset;
    if (soil_sensor) {
        if (duration_ms >= 5000) return ButtonAction::CalibrateWet;
        if (duration_ms >= 2000) return ButtonAction::CalibrateDry;
        return duration_ms < 1000 ? ButtonAction::TestContact : ButtonAction::Wake;
    }
    if (duration_ms >= 3000) return ButtonAction::Commission;
    if (duration_ms < 1000) return ButtonAction::TestContact;
    return ButtonAction::Wake;
}

// Visual cues share the exact action boundaries. No blocking delays in callbacks.
constexpr bool button_led_on(int64_t held_ms)
{
    const auto action = action_on_release(held_ms, true);
    if (action == ButtonAction::FactoryReset) return ((held_ms - 10000) / 100) % 2 == 0;
    if (action == ButtonAction::CalibrateWet) {
        const int64_t elapsed = held_ms - 5000;
        return elapsed < 600 && (elapsed / 200) % 2 == 0;
    }
    return action == ButtonAction::CalibrateDry && held_ms < 2300;
}
}
