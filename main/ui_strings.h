#pragma once

// ============================================================
// TRANSLATABLE INTERFACE TEXT
//
// Only strings the firmware itself produces live here. Anything
// that belongs to the badge owner - the avatar caption, the status
// badge, profile URLs, social network labels - stays in
// user_profile.h and socials.h, because those are content rather
// than translation.
//
// The language is chosen at compile time. Runtime switching would
// need a language picker, a full-screen redraw and persistent
// storage for the choice, which is a lot of machinery for a setting
// nobody changes twice.
//
// Note that picking a language saves almost no flash: all strings
// here add up to a couple hundred bytes, while the embedded OTF
// font dominates the binary regardless. If you need a smaller
// image, subset the font instead.
// ============================================================

#define UI_LANG_JA 1
#define UI_LANG_EN 2

#ifndef UI_LANG
#define UI_LANG UI_LANG_JA
#endif

// The indirection through a struct is deliberate. Call sites use
// ui().battery_fmt rather than a macro, so adding runtime language
// switching later means changing ui() alone - replace the constexpr
// reference with a lookup of a mutable pointer, and every existing
// call site picks up the new value untouched.
struct UiStrings
{
    // printf format, expects one unsigned: the charge percentage.
    const char *battery_fmt;

    // Shown until the first ADC reading completes.
    const char *battery_unknown;

    // printf format, expects one string: the social network label.
    const char *qr_caption_fmt;

    // Caption used when a QR code is loaded from an external
    // command rather than from the link list.
    const char *custom_link;

    // Hint on the sleep screen for how to switch the badge back on.
    const char *sleep_hint;
};

inline constexpr UiStrings UI_STRINGS_JA = {
    "電池 %u%%",
    "電池 --%",
    "%s のQR",
    "カスタムリンク",
    "ボタンで起動",
};

inline constexpr UiStrings UI_STRINGS_EN = {
    "Battery %u%%",
    "Battery --%",
    "%s QR",
    "Custom link",
    "Press button to wake",
};

// Adding a language: define UI_STRINGS_XX above with every field
// filled in, then add a branch here. The compiler will tell you
// about missing fields - the struct has no defaults on purpose.
inline constexpr const UiStrings &ui()
{
#if UI_LANG == UI_LANG_EN
    return UI_STRINGS_EN;
#else
    return UI_STRINGS_JA;
#endif
}