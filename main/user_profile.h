#pragma once

// ============================================================
// EVERYTHING PERSONAL LIVES HERE.
//
// This is the only file you have to edit after cloning the
// repository. Nothing here is translated automatically: these
// values are your content, so write them in whichever language
// you want the badge to display. The interface language is a
// separate setting - see ui_strings.h.
//
// Changing any text here requires rebuilding the font subset:
//   cd fonts && python3 build_font.py NotoSansJP-Light.ttf
// ============================================================

namespace UserProfile
{
    // Caption shown under the avatar on the main screen.
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

    // Caption on the sleep screen, under the QR code.
    //
    // The badge shows this while powered off, so it is what people
    // read when they pick it up off a table. A name or handle works
    // better here than a job title.
    constexpr const char *SLEEP_SIGN = "@your_handle";

    // Which entry of SOCIAL_LINKS the sleep screen encodes.
    // Index into the table in socials.h, 0 is the first row.
    //
    // E-ink keeps the image with no power, so this QR stays scannable
    // while the badge is switched off - pick the link that matters
    // most.
    constexpr int SLEEP_LINK_INDEX = 0;

    // Decorative frame around the avatar, used on both screens.
    //
    // Limited to what LVGL styles can express on a monochrome panel:
    // thickness, corner radius, and an optional second outline.
    // Patterned or ornamental frames would need an image with an
    // alpha channel, which the L8 render format does not carry -
    // supporting one would mean reworking the entire flush pipeline
    // for a purely decorative feature.
    //
    // A fully circular frame is deliberately absent. LV_RADIUS_CIRCLE
    // forces LVGL to build a software corner mask over the whole
    // avatar on every frame, which at 80 MHz starves the idle task
    // and trips the task watchdog. If you want a round avatar, crop
    // the source PNG to a circle with white corners when you
    // generate avatar.c - the corners vanish against the white
    // background and cost nothing at runtime.
    enum class AvatarFrame
    {
        NONE,   // no border at all
        THIN,   // 2 px, rounded
        THICK,  // 6 px, rounded
        SQUARE, // 3 px, sharp corners
        DOUBLE  // 2 px inner border plus a second outline
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