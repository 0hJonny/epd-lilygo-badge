#include "app_types.h"
#include "esp_attr.h"

// ============================================================
// GLOBAL STATE - the only definitions.
//
// Both live in RTC slow memory, so they survive deep sleep and a
// software reset: after waking, the badge restores the screen
// orientation and the selected link instead of resetting to
// defaults.
//
// They do NOT survive a power cycle, which is intentional and also
// useful for diagnostics: if these values reset, the device lost
// power rather than rebooting itself.
// ============================================================
RTC_DATA_ATTR DisplayRotation g_rotation = DisplayRotation::ROT_0;
RTC_DATA_ATTR int g_selected_social_index = 0;