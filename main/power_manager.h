#pragma once

#include <stdint.h>
#include <stdbool.h>

// ============================================================
// POWER MANAGEMENT
//
// Split out of DisplayEngine: entering sleep, configuring wake
// sources and driving RTC GPIOs is system power management, not
// rendering.
// ============================================================

// A short light sleep. Peripherals and RAM - including the PSRAM
// holding the LVGL frame buffer - are retained, the CPU stops, and
// execution resumes in place after duration_ms.
//
// Used as an idle tick: sleep 150 ms, poll the touch controller over
// I2C, sleep again.
void power_light_sleep_ms(uint32_t duration_ms);

// Full deep sleep, waking only on the BOOT button.
//
// NOT called by the firmware. Touch cannot wake the device from deep
// sleep: GPIO47 is outside the ESP32-S3 RTC domain, which covers
// only GPIO0-21. In a closed enclosure the BOOT button is
// unreachable, so entering deep sleep would leave the badge
// unresponsive until the case is opened.
//
// Kept for a future long-term storage mode, where the trade-off
// would be deliberate. Does not return.
void power_enter_deep_sleep();