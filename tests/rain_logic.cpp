#include "rain_logic.h"
#include <cassert>

using base::rain::next_state;
using base::rain::trimmed_mean_mv;

static_assert(next_state(false, 1000, 1500, 2000) == true);
static_assert(next_state(false, 1700, 1500, 2000) == false);
static_assert(next_state(true, 1700, 1500, 2000) == true);  // inside hysteresis band
static_assert(next_state(true, 2100, 1500, 2000) == false);
static_assert(next_state(false, 1500, 1500, 2000) == false); // threshold itself is not wet

int main()
{
    int a[] = {2000, 2010, 1990, 2005, 1995};
    assert(trimmed_mean_mv(a, 5) == 2000);
    int outlier[] = {2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 0};
    assert(trimmed_mean_mv(outlier, 10) == 2000); // the 0 mV spike is dropped
    int few[] = {1000, 2000};
    assert(trimmed_mean_mv(few, 2) == 0);
    return 0;
}
