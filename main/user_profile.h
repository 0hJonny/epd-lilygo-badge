#pragma once

// ============================================================
// EVERYTHING PERSONAL LIVES HERE.
//
// This is the only file you have to edit after cloning the
// repository. Nothing here is translated automatically: these
// values are your content, so write them in whichever language
// you want the badge to display. The interface language is a
// separate setting - see ui_strings.h.
// ============================================================

namespace UserProfile
{
    // Caption shown under the avatar.
    // Long strings wrap onto a second line and eat into the space
    // reserved for the QR code, so keep it to a few words.
    constexpr const char *PHOTO_SIGN = "バックエンドエンジニア";

    // Short tag in the top-left corner of the status bar, rendered
    // as a small chip. Meant for a conference abbreviation or your
    // initials.
    //
    // Keep it to one or two characters. The chip is a fixed 40x40
    // square: longer text will be clipped, and in portrait mode it
    // also crowds the battery readout.
    constexpr const char *STATUS_BADGE = "TC";

    // Decorative frame around the avatar.
    //
    // Limited to what LVGL styles can express on a monochrome panel:
    // thickness, corner radius, and an optional second outline.
    // Pattern or ornament borders would need an image with an alpha
    // channel, which the L8 render format does not carry - adding
    // one would mean reworking the whole flush pipeline for a purely
    // decorative feature.
    enum class AvatarFrame
    {
        NONE,
        THIN,
        THICK,
        SQUARE,
        DOUBLE
    };

    constexpr AvatarFrame PHOTO_FRAME = AvatarFrame::DOUBLE;
}

// ============================================================
// PROFILE LINKS
//
// The selected link is what gets encoded into the QR code.
// Every entry is guarded with #ifndef, so a build system or a
// private header may override any of them without editing this
// file - useful if you want to keep real handles out of a public
// fork.
// ============================================================
#ifndef DEFAULT_URL_X
#define DEFAULT_URL_X "https://x.com/your_handle"
#endif
#ifndef DEFAULT_URL_CONNPASS
#define DEFAULT_URL_CONNPASS "https://connpass.com/user/your_handle/"
#endif
#ifndef DEFAULT_URL_LINKEDIN
#define DEFAULT_URL_LINKEDIN "https://linkedin.com/in/your_handle"
#endif
#ifndef DEFAULT_URL_GITHUB
#define DEFAULT_URL_GITHUB "https://github.com/your_handle"
#endif
#ifndef DEFAULT_URL_INSTAGRAM
#define DEFAULT_URL_INSTAGRAM "https://instagram.com/your_handle"
#endif
#ifndef DEFAULT_URL_DISCORD
#define DEFAULT_URL_DISCORD "https://discord.gg/your_invite"
#endif
#ifndef DEFAULT_URL_PROJECT
#define DEFAULT_URL_PROJECT "https://your-project.example"
#endif