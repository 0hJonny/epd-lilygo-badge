#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "lvgl.h"

// ============================================================
// SHARED TYPES AND GLOBAL STATE
// ============================================================

// Commands posted to the display task's queue.
//
// LVGL is not thread-safe, so every widget call has to happen on
// DisplayTask. Anything originating elsewhere - a button callback,
// the battery timer - goes through this queue instead of touching
// LVGL directly.
enum class CommandType
{
    // Show a QR code for an arbitrary URL carried in the payload.
    // Nothing sends this yet; it is the extension point for
    // displaying links that are not in SOCIAL_LINKS.
    LOAD_SCENE_PROFILE,

    // Payload carries a preformatted battery string.
    UPDATE_BATTERY,

    // Rotate the screen 90 degrees clockwise.
    CYCLE_ORIENTATION,

    // Force a full panel refresh. Bound to a long press on the
    // rotate button: individual region clears do not remove
    // cumulative particle drift, and this is the manual escape
    // hatch for when smears become noticeable before the automatic
    // refresh counter fires.
    FULL_REFRESH
};

struct UIEvent
{
    CommandType command;

    // Heap-allocated with strdup by the sender; the display task
    // frees it after handling. Null when the command carries no data.
    char *payload;
};

enum class DisplayRotation
{
    ROT_0,
    ROT_90,
    ROT_180,
    ROT_270
};

// Defined in app_state.cpp with RTC_DATA_ATTR so both survive
// sleep and a software reset. The section attribute is intentionally
// not repeated here: a declaration does not need it, and duplicating
// it produces warnings on some toolchain versions.
extern DisplayRotation g_rotation;
extern int g_selected_social_index;

inline bool is_portrait_mode()
{
    return (g_rotation == DisplayRotation::ROT_90 || g_rotation == DisplayRotation::ROT_270);
}

// Passed down to every UI component at construction.
struct UIContext
{
    lv_font_t *font_24 = nullptr;
    lv_font_t *font_48 = nullptr;
    QueueHandle_t app_queue = nullptr;
};