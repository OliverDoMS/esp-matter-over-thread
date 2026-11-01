#pragma once

#include "button_actions.h"
#include <esp_err.h>

namespace base {
using ButtonCallback = void (*)(ButtonAction);

// No permanent polling loop: the GPIO button driver supports automatic sleep.
esp_err_t init_board(ButtonCallback callback);
}
