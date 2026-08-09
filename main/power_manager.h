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

// Configure the manual sleep button as an input with a pull-up.
// Call once at startup, before polling it.
void power_button_init();

// True while the sleep button is physically held down.
bool power_button_pressed();

// Block until the button is released, then debounce.
//
// Required right after waking from deep sleep: the button is still
// held at that moment, and arming the sleep trigger immediately would
// send the device straight back to sleep on the same press.
void power_wait_button_release();

// True if this boot was caused by the sleep button rather than a
// power-on or a reset.
bool power_woke_from_button();

// A short light sleep. Peripherals and RAM - including the PSRAM
// holding the LVGL frame buffer - are retained, the CPU stops, and
// execution resumes in place after duration_ms.
//
// Used as an idle tick: sleep 150 ms, poll the touch controller over
// I2C, sleep again.
void power_light_sleep_ms(uint32_t duration_ms);

// Full deep sleep. Wakes only on the sleep button.
//
// Touch cannot wake from deep sleep: GPIO47 is outside the ESP32-S3
// RTC domain, which covers only GPIO0-21. That is why the button
// exists at all, and why the caller must render a "sleeping" screen
// first - the panel keeps showing it, so the badge has to look
// deliberately off rather than frozen.
//
// Does not return.
void power_enter_deep_sleep();