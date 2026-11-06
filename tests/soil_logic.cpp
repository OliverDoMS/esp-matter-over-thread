// Host test for the soil placeholder and result-mode logic; no SDK needed.
#include "soil_math.h"
#include "soil_settings.h"

#include <cstdio>
#include <cstring>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL line %d: %s\n", __LINE__, #cond); ++failures; } } while (0)

int main()
{
    using namespace base::soil;
    // Calibration progress is visible through distinct placeholders.
    CHECK(published_hundredths(-1, -1, -1) == 123);
    CHECK(published_hundredths(-1, 2900, -1) == 234);
    CHECK(published_hundredths(-1, -1, 1400) == 345);
    // Calibrated: the real value, or unknown (-1) on a failed reading.
    CHECK(published_hundredths(5000, 2900, 1400) == 5000);
    CHECK(published_hundredths(-1, 2900, 1400) == -1);
    // Both set but too close is not calibrated; calibrate() rejects this pair anyway.
    CHECK(published_hundredths(-1, 1450, 1400) == 123);
    CHECK(placeholder_hundredths(2900, 1400) == -1);
    CHECK(moisture_hundredths(1400, 2900, 1400) == 10000);
    CHECK(moisture_hundredths(2900, 2900, 1400) == 0);

    // Result modes follow the action modes and never trigger an action.
    CHECK(result_mode(Result::Ok) == kActionCount);
    CHECK(result_mode(Result::Busy) == kModeCount - 1);
    CHECK(not_connected(0) == Result::NotConnected1);
    CHECK(not_connected(2) == Result::NotConnected3);
    CHECK(std::strcmp(mode_label(result_mode(not_connected(1))), "Fehler: Sensor 2 nicht erkannt") == 0);
    CHECK(std::strcmp(mode_label(kMeasureMode), "Jetzt messen") == 0);
    for (uint8_t mode = 0; mode < kModeCount; ++mode) CHECK(std::strlen(mode_label(mode)) <= 64); // Matter label limit
    CHECK(kSensorCount == 3);

    // Watering threshold: only a calibrated, valid reading below it reports "dry".
    CHECK(moist_enough(900, 2900, 1400, 10) == false);
    CHECK(moist_enough(1000, 2900, 1400, 10) == true);
    CHECK(moist_enough(-1, 2900, 1400, 10) == true);
    CHECK(moist_enough(123, -1, -1, 10) == true);
    CHECK(choice_mode(kWaterBelow, kDefaultWaterBelow) == 1);
    CHECK(choice_mode(kWetAbove, kDefaultWetAbove) == 3);
    CHECK(choice_mode(kSamples, kDefaultSamples) == 3);
    CHECK(choice_mode(kSamples, 7) == -1);
    CHECK(trimmed_per_side(32) == 6);   // unchanged behaviour: middle 20 of 32
    CHECK(trimmed_per_side(4) == 0);
    for (const auto &option : kSamples) CHECK(option.value <= kMaxSamples && option.value - 2 * trimmed_per_side(option.value) > 0);

    if (failures == 0) std::printf("soil_logic: all checks passed\n");
    return failures == 0 ? 0 : 1;
}
