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
void gt911_sleep();
void lv_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data);