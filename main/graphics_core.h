#pragma once

#include <stdbool.h>
#include "lvgl.h"
#include "app_types.h"

// ==========================================================
// 2. E-INK AND LVGL DRIVER CALLBACKS
// ==========================================================

// One-time allocation of working buffers in PSRAM. Must be called once
// after lv_init() and before the first render. Returns false if
// out of memory — execution cannot continue in this case.
bool graphics_core_init();

// Aligns the invalidation area to even boundaries — a requirement
// of the 4bpp packing in epd_draw_grayscale_image().
void rounder_event_cb(lv_event_t *e);

// Maps our custom orientation enum to the LVGL enum.
lv_display_rotation_t to_lv_rotation(DisplayRotation rot);

// flush_cb for lv_display: rotation -> pack L8 to 4bpp -> output to EPD.
//
// Each area is prepared using epd_clear_area() before writing.
// This is not a cosmetic feature, but a basic panel requirement:
// epd_draw_grayscale_image() pushes particles based on their CURRENT
// state, rather than setting an absolute value.
void epd_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);

// ----------------------------------------------------------
// PERIODIC FULL REFRESH
//
// epd_clear_area() prepares each area before writing, but does not
// eliminate accumulated particle drift: slight ghosting/artifacts appear
// after a few dozen redraws. An occasional full epd_clear() is required.
//
// THE CONTRACT that the calling code must strictly follow:
// The driver only REQUESTS a refresh by raising a flag. The rendering engine
// must regularly check graphics_core_is_full_refresh_pending() and,
// if triggered, invalidate the ENTIRE screen. Otherwise, the clearing
// in epd_flush_cb will erase the whole panel, but only the area
// LVGL considers dirty will be drawn — causing the UI to disappear
// until the next full update.
// ----------------------------------------------------------

// Request a full refresh on the next frame.
void graphics_core_request_full_refresh();

// Returns true if a refresh is requested but not yet executed.
bool graphics_core_is_full_refresh_pending();