#include "graphics_core.h"

#include <stdint.h>
#include "esp_heap_caps.h"
#include "esp_log.h"

#include "app_config.h"

extern "C"
{
#include "epd_driver.h"
}

static const char *TAG = "GFX_CORE";

// ============================================================
// WORKING BUFFERS
//
// Allocated once at startup. These used to be malloc'd and freed on
// every flush, which fragmented the PSRAM heap, cost milliseconds
// per frame, and would eventually fail outright on a device running
// for days. The maximum sizes are known at compile time, so there
// is nothing to compute at runtime.
// ============================================================

// Rotation scratch: a full frame in LVGL's format (L8, one byte per
// pixel).
static uint8_t *s_rotate_scratch = nullptr;
static uint32_t s_rotate_scratch_size = 0;

// Panel output: 4 bits per pixel, two pixels packed per byte.
static uint8_t *s_epd_buf = nullptr;
static uint32_t s_epd_buf_size = 0;

// ============================================================
// FULL REFRESH STATE
// ============================================================

// Frames drawn since the last full clear.
static uint32_t s_updates_since_clear = 0;

// A full refresh has been requested but not performed yet.
static bool s_full_refresh_pending = false;

// True while inside the sequence of flushes making up one frame.
// Needed to tell the first flush from the rest: LVGL only signals
// the last one, via lv_display_flush_is_last().
static bool s_frame_in_progress = false;

// True when epd_clear() ran at the start of the current frame.
// The entire panel is already white in that case, so the per-region
// epd_clear_area() calls below would repeat work that is already
// done - and a region clear is the slowest operation the panel
// performs.
static bool s_frame_pre_cleared = false;

void graphics_core_request_full_refresh()
{
    s_full_refresh_pending = true;
}

bool graphics_core_is_full_refresh_pending()
{
    return s_full_refresh_pending;
}

bool graphics_core_init()
{
    if (s_rotate_scratch && s_epd_buf)
        return true;

    // Widest possible stride for a full-frame L8 buffer.
    uint32_t max_stride = lv_draw_buf_width_to_stride(PHYS_W, LV_COLOR_FORMAT_L8);
    s_rotate_scratch_size = max_stride * (uint32_t)PHYS_H;

    // 4bpp: two pixels per byte, rounding the width up.
    s_epd_buf_size = (uint32_t)((PHYS_W + 1) / 2) * (uint32_t)PHYS_H;

    s_rotate_scratch = (uint8_t *)heap_caps_malloc(s_rotate_scratch_size,
                                                   MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_rotate_scratch)
    {
        ESP_LOGE(TAG, "Failed to allocate rotation scratch buffer (%lu bytes)",
                 (unsigned long)s_rotate_scratch_size);
        return false;
    }

    s_epd_buf = (uint8_t *)heap_caps_malloc(s_epd_buf_size,
                                            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_epd_buf)
    {
        ESP_LOGE(TAG, "Failed to allocate EPD output buffer (%lu bytes)",
                 (unsigned long)s_epd_buf_size);
        heap_caps_free(s_rotate_scratch);
        s_rotate_scratch = nullptr;
        return false;
    }

    ESP_LOGI(TAG, "Drawing buffers allocated: rotation %lu B, EPD %lu B",
             (unsigned long)s_rotate_scratch_size, (unsigned long)s_epd_buf_size);
    return true;
}

void rounder_event_cb(lv_event_t *e)
{
    // epd_draw_grayscale_image() packs two pixels per byte, so every
    // region has to start and end on an even coordinate.
    lv_area_t *area = (lv_area_t *)lv_event_get_param(e);
    area->x1 &= ~1;
    area->x2 |= 1;
    area->y1 &= ~1;
    area->y2 |= 1;
}

lv_display_rotation_t to_lv_rotation(DisplayRotation rot)
{
    switch (rot)
    {
    case DisplayRotation::ROT_0:
        return LV_DISPLAY_ROTATION_0;
    case DisplayRotation::ROT_90:
        return LV_DISPLAY_ROTATION_90;
    case DisplayRotation::ROT_180:
        return LV_DISPLAY_ROTATION_180;
    case DisplayRotation::ROT_270:
        return LV_DISPLAY_ROTATION_270;
    }
    return LV_DISPLAY_ROTATION_0;
}

void epd_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    if (!s_rotate_scratch || !s_epd_buf)
    {
        ESP_LOGE(TAG, "epd_flush_cb called before graphics_core_init");
        lv_display_flush_ready(disp);
        return;
    }

    epd_poweron();

    lv_color_format_t cf = lv_display_get_color_format(disp);
    lv_display_rotation_t rotation = lv_display_get_rotation(disp);

    // Start of a new frame. If a full refresh was requested, wipe the
    // panel BEFORE drawing - doing it afterwards would erase the
    // image that was just written.
    //
    // The flag is consumed here, so the display engine only needs to
    // invalidate the whole screen once.
    if (!s_frame_in_progress)
    {
        s_frame_in_progress = true;
        s_frame_pre_cleared = false;

        if (s_full_refresh_pending)
        {
            ESP_LOGI(TAG, "Full panel refresh");
            epd_clear();
            s_full_refresh_pending = false;
            s_updates_since_clear = 0;
            s_frame_pre_cleared = true;
        }
    }

    lv_area_t phys_area = *area;
    uint8_t *phys_px_map = px_map;
    uint32_t phys_stride;

    if (rotation != LV_DISPLAY_ROTATION_0)
    {
        lv_display_rotate_area(disp, &phys_area);

        uint32_t src_stride = lv_draw_buf_width_to_stride(lv_area_get_width(area), cf);
        phys_stride = lv_draw_buf_width_to_stride(lv_area_get_width(&phys_area), cf);

        lv_draw_sw_rotate(px_map, s_rotate_scratch,
                          lv_area_get_width(area), lv_area_get_height(area),
                          src_stride, phys_stride, rotation, cf);

        phys_px_map = s_rotate_scratch;
    }
    else
    {
        phys_stride = lv_draw_buf_width_to_stride(lv_area_get_width(area), cf);
    }

    int32_t phys_x = phys_area.x1;
    int32_t phys_y = phys_area.y1;
    int32_t phys_w = lv_area_get_width(&phys_area);
    int32_t phys_h = lv_area_get_height(&phys_area);

    uint32_t stride = (uint32_t)((phys_w + 1) / 2);
    uint32_t needed = stride * (uint32_t)phys_h;

    if (needed > s_epd_buf_size)
    {
        ESP_LOGE(TAG, "Region %ldx%ld exceeds EPD buffer (%lu > %lu)",
                 (long)phys_w, (long)phys_h,
                 (unsigned long)needed, (unsigned long)s_epd_buf_size);
        lv_display_flush_ready(disp);
        return;
    }

    // Pack L8 down to 4bpp: the panel takes two pixels per byte,
    // low nibble first. Odd trailing pixels are padded with white.
    uint32_t i = 0;
    for (int y = 0; y < phys_h; y++)
    {
        uint8_t *row = phys_px_map + (uint32_t)y * phys_stride;
        for (int x = 0; x < phys_w; x += 2)
        {
            uint8_t p1 = row[x] >> 4;
            uint8_t p2 = (x + 1 < phys_w) ? (row[x + 1] >> 4) : 0x0F;

            s_epd_buf[i++] = p1 | (p2 << 4);
        }
    }

    // Useful when debugging layout: if 960x540 shows up here on an
    // ordinary tap, LVGL is invalidating the whole screen and the fix
    // belongs in the widget tree, not in this driver.
    ESP_LOGD(TAG, "flush %ldx%ld @ (%ld,%ld)",
             (long)phys_w, (long)phys_h, (long)phys_x, (long)phys_y);

    Rect_t epd_area = {.x = phys_x, .y = phys_y, .width = phys_w, .height = phys_h};

    // Clearing a region before writing it is mandatory, not cosmetic:
    // epd_draw_grayscale_image() drives particles from their current
    // state rather than setting an absolute value, so without this
    // the new image superimposes onto the old one.
    //
    // The exception is a frame that already began with epd_clear() -
    // the panel is white everywhere, and repeating the clear per
    // region would double the slowest part of a full refresh.
    if (!s_frame_pre_cleared)
    {
        epd_clear_area(epd_area);
    }
    epd_draw_grayscale_image(epd_area, s_epd_buf);

    if (lv_display_flush_is_last(disp))
    {
        s_frame_in_progress = false;
        s_frame_pre_cleared = false;

#if RENDER_MODE == RENDER_MODE_PARTIAL
        if (++s_updates_since_clear >= EINK_GHOST_CLEAR_INTERVAL)
        {
            // Request only - the clear itself happens before the next
            // frame is drawn, and that frame has to be a full one.
            ESP_LOGI(TAG, "Full refresh requested after %lu updates",
                     (unsigned long)s_updates_since_clear);
            graphics_core_request_full_refresh();
        }
#endif
        epd_poweroff_all();
    }
    lv_display_flush_ready(disp);
}