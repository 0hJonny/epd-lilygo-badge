#pragma once

#include "lvgl.h"

// ============================================================
// GT911 CAPACITIVE TOUCH DRIVER (I2C)
//
// Only the minimum is exported:
//   i2c_bus_init()   - bring up the bus, latch the address, verify
//   gt911_sleep()    - put the controller to sleep before deep sleep
//   lv_touchpad_read - read callback for lv_indev
//
// i2c_scanner() and gt911_read_touch() stay static in the .cpp.
// ============================================================

void i2c_bus_init();

// Put the controller into sleep mode.
//
// The INT line must be driven low before the command, per the Goodix
// programming guide - without that the controller ignores it and keeps
// running, drawing milliamps for the entire sleep. INT is left low on
// return so the part stays down; the caller is expected to latch it if
// entering deep sleep.
void gt911_sleep();

void lv_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data);

// Switch off the PCF8563 CLKOUT square wave.
//
// The RTC shares this I2C bus but is otherwise unused. Called from
// i2c_bus_init(); see RTC_DISABLE_CLKOUT in app_config.h.
//
// Harmless if the part is absent - the write simply fails and is
// logged.
void rtc_disable_clkout();