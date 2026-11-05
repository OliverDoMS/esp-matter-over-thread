#pragma once
#include <cstdint>

namespace base {
enum class ButtonAction : intptr_t { Wake, TestContact, Commission, FactoryReset };

constexpr ButtonAction action_on_release(int64_t duration_ms, bool soil_sensor = false)
{
    if (duration_ms >= 10000) return ButtonAction::FactoryReset;
    // With several soil sensors BOOT cannot select one; calibration runs over Matter.
    if (soil_sensor) return duration_ms < 1000 ? ButtonAction::TestContact : ButtonAction::Wake;
    if (duration_ms >= 3000) return ButtonAction::Commission;
    if (duration_ms < 1000) return ButtonAction::TestContact;
    return ButtonAction::Wake;
}

// Visual cues share the exact action boundaries. No blocking delays in callbacks.
constexpr bool button_led_on(int64_t held_ms)
{
    return action_on_release(held_ms, true) == ButtonAction::FactoryReset && ((held_ms - 10000) / 100) % 2 == 0;
}
}
