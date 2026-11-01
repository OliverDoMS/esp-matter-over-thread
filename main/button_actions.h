#pragma once
#include <cstdint>

namespace base {
enum class ButtonAction : intptr_t { Wake, TestContact, Commission, FactoryReset };

constexpr ButtonAction action_on_release(int64_t duration_ms)
{
    if (duration_ms >= 10000) return ButtonAction::FactoryReset;
    if (duration_ms >= 3000) return ButtonAction::Commission;
    if (duration_ms < 1000) return ButtonAction::TestContact;
    return ButtonAction::Wake;
}
}
