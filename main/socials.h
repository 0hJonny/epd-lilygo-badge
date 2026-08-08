#pragma once

#include "user_profile.h"

// ============================================================
// SOCIAL LINK TABLE
//
// The URLs themselves live in user_profile.h. What stays here is
// the shape of the list: which entries exist, in what order, and
// how each one is labelled.
//
// The labels are content rather than interface text, so they are
// not routed through ui_strings.h - a Japanese badge may well want
// English network names, and that is the owner's call.
// ============================================================

struct SocialLinkDef
{
    // Text shown in the list row.
    const char *label;

    // One or two characters drawn inside the small square chip.
    //
    // Real brand logos are deliberately avoided: they are somebody
    // else's trademarks, and at chip size on a grayscale e-ink
    // panel they turn into unreadable blobs anyway.
    const char *monogram;

    // Encoded into the QR code when this row is selected.
    const char *url;
};

// The table is inline constexpr and lives in the header so that
// SOCIAL_LINKS_COUNT can be derived with sizeof instead of being
// hard-coded. SocialPanel needs that count at compile time for its
// fixed-size row array.
//
// Rows can be added or removed freely. Beyond roughly eight entries
// the list stops fitting on screen in landscape orientation.
inline constexpr SocialLinkDef SOCIAL_LINKS[] = {
    {"X", "X", DEFAULT_URL_X},
    {"connpass", "cp", DEFAULT_URL_CONNPASS},
    {"LinkedIn", "in", DEFAULT_URL_LINKEDIN},
    {"GitHub", "gh", DEFAULT_URL_GITHUB},
    {"Instagram", "ig", DEFAULT_URL_INSTAGRAM},
    {"Discord", "dc", DEFAULT_URL_DISCORD},
    {"プロジェクトページ", "@", DEFAULT_URL_PROJECT},
};

inline constexpr int SOCIAL_LINKS_COUNT = sizeof(SOCIAL_LINKS) / sizeof(SOCIAL_LINKS[0]);