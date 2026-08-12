#pragma once

#include "driver/i2c.h"
#include "ui_strings.h"

// ============================================================
// HARDWARE AND TIMING CONFIGURATION
//
// Everything here describes the board or tunes behaviour. Nothing
// personal lives in this file - see user_profile.h for that.
// ============================================================

// ------------------------------------------------------------
// Panel geometry: ED047TC1, fixed by the hardware.
// ------------------------------------------------------------
#define PHYS_W 960
#define PHYS_H 540

// ------------------------------------------------------------
// Display task
//
// The stack is generous because LVGL rendering and the tiny_ttf
// glyph rasteriser both recurse.
// ------------------------------------------------------------
#define DISPLAY_QUEUE_LEN 10
#define DISPLAY_STACK_SIZE 16384

// ------------------------------------------------------------
// Render mode
//
// PARTIAL redraws only the regions LVGL marked dirty. That is what
// this firmware is tuned for: tapping a link repaints two list rows
// and the QR, not the whole screen.
//
// FULL renders the entire 960x540 frame on every change. Slower and
// heavier on the panel, but immune to any partial-update artefact -
// worth switching to if regional refresh looks wrong on your unit.
//
// In FULL mode every frame is already a full frame, so the periodic
// whole-screen clear is skipped and EINK_GHOST_CLEAR_INTERVAL is
// unused.
// ------------------------------------------------------------
#define RENDER_MODE_PARTIAL 1
#define RENDER_MODE_FULL 2

#ifndef RENDER_MODE
#define RENDER_MODE RENDER_MODE_PARTIAL
#endif

#if RENDER_MODE == RENDER_MODE_FULL
#define LV_RENDER_MODE LV_DISPLAY_RENDER_MODE_FULL
#else
#define LV_RENDER_MODE LV_DISPLAY_RENDER_MODE_PARTIAL
#endif

// ------------------------------------------------------------
// How many frames between automatic full panel refreshes.
//
// epd_clear_area() prepares each region before it is written, but it
// does not undo cumulative particle drift - faint smears build up
// over dozens of redraws. A full epd_clear() removes them.
//
// Lower values mean a cleaner panel and more flashing. A long press
// on the rotate button triggers the same refresh manually, so this
// only needs to catch drift the user has not noticed yet.
// ------------------------------------------------------------
#define EINK_GHOST_CLEAR_INTERVAL 40

// ------------------------------------------------------------
// Link list metrics.
//
// If the list overflows or leaves too much empty space on your panel,
// these are the first numbers to adjust.
// ------------------------------------------------------------
#define SOCIAL_ROW_HEIGHT_LANDSCAPE 47
#define SOCIAL_ROW_HEIGHT_PORTRAIT 45
#define SOCIAL_ROW_GAP 8

// ------------------------------------------------------------
// Fixed width of the battery label in the status bar.
//
// Must fit the longest string ui().battery_fmt can produce, which
// depends on the interface language. Too small and the text clips;
// too large and the rotate button drifts toward the centre.
//
// A fixed width also stops the flex row reflowing when the text
// changes, which would repaint the button on every battery update.
// ------------------------------------------------------------
#if UI_LANG == UI_LANG_EN
#define STATUS_BATTERY_LABEL_WIDTH 170
#else
#define STATUS_BATTERY_LABEL_WIDTH 130
#endif

// ============================================================
// PIN ASSIGNMENT - LILYGO T5-4.7-S3
// ============================================================
#define I2C_MASTER_SCL_IO 17
#define I2C_MASTER_SDA_IO 18
#define I2C_MASTER_NUM I2C_NUM_0

// GT911 address is selected at reset by the level on the INT pin.
// If the I2C scanner reports 0x14 instead, change this.
#define GT911_ADDR 0x5D
#define TOUCH_INT_PIN 47

// PCF8563 real-time clock, same bus as the touch controller.
#define PCF8563_ADDR 0x51

// BOOT button. Not used - see SLEEP_BUTTON_PIN.
#define WAKEUP_BUTTON_PIN 0

// ============================================================
// BATTERY MEASUREMENT
//
// Verify the pin against the schematic for YOUR board revision.
// On T5-4.7-S3 (V2.4) it is GPIO14, which maps to ADC2 channel 3.
// Both defines below must change together.
//
// ADC2 channel mapping on ESP32-S3:
//   GPIO11=CH0  GPIO12=CH1  GPIO13=CH2  GPIO14=CH3
//   GPIO15=CH4  GPIO16=CH5  GPIO17=CH6  GPIO18=CH7
// ============================================================
#define BATTERY_ADC_GPIO 14
#define BATTERY_ADC_CHANNEL ADC2_CHANNEL_3

// The board divides battery voltage by two before the ADC.
#define BATTERY_DIVIDER_RATIO 2

// Samples averaged per reading, to suppress ADC noise.
#define BATTERY_ADC_SAMPLES 16

// How often to sample. Each reading costs a wake from light sleep,
// and e-ink should not be redrawn often anyway, so this is
// deliberately slow.
#define BATTERY_POLL_INTERVAL_MS 60000

// Li-Po limits used by the discharge curve.
#define BATTERY_MV_FULL 4200
#define BATTERY_MV_EMPTY 3300

// ============================================================
// BATTERY PROTECTION
//
// Below this charge level the badge forces itself into deep sleep
// instead of continuing to idle. Li-Po cells degrade when taken to
// full discharge, and idle current alone will flatten one over a few
// weeks of storage.
//
// Set to 0 to disable the protection entirely.
//
// The check runs only after a successful ADC reading, so a board with
// no battery attached - or with the ADC misconfigured - never trips
// it accidentally.
// ============================================================
#define BATTERY_CRITICAL_PERCENT 5

// ============================================================
// REAL-TIME CLOCK
//
// The board carries a PCF8563 at I2C 0x51, backed by a rechargeable
// MS412 cell. This firmware does not use it for timekeeping.
//
// Its CLKOUT pin emits a 32.768 kHz square wave by default and keeps
// doing so forever. On an open-drain output with a pull-up that costs
// a few hundred microamps continuously, and it drains the backup cell
// whenever main power is absent - NXP ties multi-week backup
// operation to having CLKOUT switched off.
//
// Disabling it is one-way safe: the datasheet specifies the pin goes
// high-impedance rather than resting at a level.
//
// Set to 0 if you intend to use CLKOUT as a clock source for
// something, or would rather leave the RTC untouched.
// ============================================================
#define RTC_DISABLE_CLKOUT 1

// ============================================================
// SLEEP
// ============================================================

// Inactivity before entering the light-sleep power-saving loop.
#define SLEEP_IDLE_TIMEOUT_MS 30000

// Wake interval while in that loop, used to poll the touch
// controller. This is also the worst-case response latency to a tap.
// Lower is more responsive but draws more average current; 150 ms is
// invisible next to the hundreds of milliseconds an e-ink redraw
// takes.
#define SLEEP_TOUCH_POLL_MS 150

// ------------------------------------------------------------
// Manual deep sleep button
//
// IO21 rather than IO00: the latter is a strapping pin, and holding
// it low across a reset can drop the chip into download mode instead
// of running the firmware. Deep sleep wakeup goes through a reset, so
// that risk is real.
//
// IO21 also sits inside the RTC domain (GPIO0-21), which is a hard
// requirement for EXT1 wakeup. The touch controller on IO47 does not,
// which is why touch cannot wake the device from deep sleep.
//
// Verified active-low on rev V2.4: the button pulls to ground and an
// internal pull-up holds the idle state high.
// ------------------------------------------------------------
#define SLEEP_BUTTON_PIN 21

// How long the button must be held before deep sleep is triggered.
// Long enough that a stray press while handling the badge does not
// switch it off mid-conversation.
#define SLEEP_BUTTON_HOLD_MS 2000