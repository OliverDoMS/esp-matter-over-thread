#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace base::rain {
// Hardware-independent evaluation, kept header-only so it can be checked on the host.

// Mean of the samples after dropping the lowest and highest 20 % (outlier filter).
// Returns 0 if fewer than three samples are given. Sorts the array in place.
inline int trimmed_mean_mv(int *samples, size_t count)
{
    if (count < 3) return 0;
    std::sort(samples, samples + count);
    const size_t trim = count / 5;
    long sum = 0;
    for (size_t i = trim; i < count - trim; i++) sum += samples[i];
    return static_cast<int>(sum / static_cast<long>(count - 2 * trim));
}

// Rain boards output a LOWER voltage when wet. Hysteresis avoids flicker at the edge:
// rain starts below wet_below_mv and ends only above dry_above_mv.
constexpr bool next_state(bool raining, int mv, int wet_below_mv, int dry_above_mv)
{
    if (!raining && mv < wet_below_mv) return true;
    if (raining && mv > dry_above_mv) return false;
    return raining;
}
}
