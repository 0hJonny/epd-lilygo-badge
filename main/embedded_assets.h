#pragma once

#include <stdint.h>
#include "lvgl.h"

// ============================================================
// EMBEDDED ASSETS
//
// The font is linked in via EMBED_FILES in main/CMakeLists.txt. The
// build derives these symbol names from the filename, replacing dots
// with underscores - renaming ui_font_jp.otf means changing both the
// CMakeLists entry and the symbols below.
//
// The file is a subset of Noto Sans JP built by fonts/build_font.py,
// carrying only the glyphs this firmware renders. It is a Modified
// Version under OFL terms and is deliberately not named after the
// original. See fonts/README.md.
// ============================================================
extern const uint8_t font_start[] asm("_binary_ui_font_jp_otf_start");
extern const uint8_t font_end[] asm("_binary_ui_font_jp_otf_end");

// Generated LVGL image array, see fonts/README.md for the avatar
// conversion notes.
LV_IMAGE_DECLARE(avatar);