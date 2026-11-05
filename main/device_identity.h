#pragma once

#include <esp_err.h>

namespace base::identity {
// Per-device Matter identity derived from the chip's unique base MAC address:
// setup passcode, discriminator, SPAKE2+ salt, serial number and unique ID.
// The derivation is mirrored in scripts/pairing_code.py, which prints the manual
// pairing code for a given MAC (for a label). Everything is reproducible, nothing is
// stored; the values are NOT secret, the BOOT-button commissioning window is the gate.

// Call before esp_matter::start().
esp_err_t install();
// Call after esp_matter::start(): writes serial number and unique ID once (skips if equal).
esp_err_t publish_names();
}
