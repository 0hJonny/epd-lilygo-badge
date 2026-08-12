#include "display_engine.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <algorithm>

#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_log.h"

#include "app_config.h"
#include "embedded_assets.h"
#include "ui_strings.h"
#include "graphics_core.h"
#include "gt911_touch.h"
#include "battery.h"
#include "power_manager.h"

extern "C"
{
#include "epd_driver.h"
}

static const char *TAG = "DISPLAY_ENGINE";

DisplayEngine::DisplayEngine(QueueHandle_t q)
    : m_queue(q), m_disp(nullptr), m_indev(nullptr),
      m_screen_root(nullptr), m_statusbar(nullptr), m_card_scene(nullptr),
      m_sleep_panel(nullptr), m_battery_timer(nullptr),
      m_battery_percent(0xFF), m_is_idle(false), m_button_down_us(0) {}

void DisplayEngine::initLvgl()
{
    ESP_LOGI(TAG, "Initialising LVGL...");
    lv_init();

    if (!graphics_core_init())
    {
        ESP_LOGE(TAG, "Failed to allocate drawing buffers");
        abort();
    }

    m_disp = lv_display_create(PHYS_W, PHYS_H);

    // L8: one byte per pixel. The panel takes 4 bits per pixel, but
    // LVGL has no 4bpp render format, so epd_flush_cb packs pairs of
    // bytes on the way out.
    lv_display_set_color_format(m_disp, LV_COLOR_FORMAT_L8);

    uint32_t buf_size = PHYS_W * PHYS_H;
    uint8_t *buf1 = (uint8_t *)heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf1)
    {
        ESP_LOGE(TAG, "Failed to allocate %lu byte frame buffer in PSRAM", (unsigned long)buf_size);
        abort();
    }

    lv_display_set_buffers(m_disp, buf1, nullptr, buf_size, LV_RENDER_MODE);
#if RENDER_MODE == RENDER_MODE_FULL
    ESP_LOGI(TAG, "Render mode: FULL - every change repaints the whole panel");
#else
    ESP_LOGI(TAG, "Render mode: PARTIAL");
#endif

    lv_display_set_flush_cb(m_disp, epd_flush_cb);
    lv_display_add_event_cb(m_disp, rounder_event_cb, LV_EVENT_INVALIDATE_AREA, NULL);

    i2c_bus_init();
    m_indev = lv_indev_create();
    lv_indev_set_type(m_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(m_indev, lv_touchpad_read);
    lv_indev_set_display(m_indev, m_disp);

    // The rotate button distinguishes a tap from a long press. The LVGL
    // default threshold is short enough to trigger by accident on a
    // panel that responds slowly, so it is raised here.
    lv_indev_set_long_press_time(m_indev, 1000);

    size_t font_size_bytes = font_end - font_start;
    m_ctx.font_24 = lv_tiny_ttf_create_data(font_start, font_size_bytes, 24);
    if (!m_ctx.font_24)
    {
        ESP_LOGE(TAG, "Failed to create font from embedded OTF");
        abort();
    }
    m_ctx.app_queue = m_queue;
    ESP_LOGI(TAG, "LVGL and fonts ready");
}

void DisplayEngine::buildUI()
{
    ESP_LOGI(TAG, "Building UI...");
    m_screen_root = lv_screen_active();
    lv_obj_set_layout(m_screen_root, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(m_screen_root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_color(m_screen_root, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(m_screen_root, LV_OPA_COVER, 0);

    lv_obj_remove_flag(m_screen_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(m_screen_root, LV_SCROLLBAR_MODE_OFF);

    m_statusbar = new StatusBar(m_screen_root, &m_ctx);

    m_card_scene = new CardScene(m_screen_root, &m_ctx);
    m_card_scene->selectSocialByIndex(g_selected_social_index);

    // Created hidden. Sits alongside the card rather than inside it, so
    // it can cover the status bar too.
    m_sleep_panel = new SleepPanel(m_screen_root, &m_ctx);

    applyOrientation();
    ESP_LOGI(TAG, "UI built");
}

void DisplayEngine::applyOrientation()
{
    bool is_portrait = is_portrait_mode();
    ESP_LOGI(TAG, "Applying orientation: %s", is_portrait ? "portrait" : "landscape");

    // LVGL rotates rendering and touch coordinates itself. The fixed
    // axis correction in lv_touchpad_read is a separate concern - see
    // the comment there before touching either.
    lv_display_set_rotation(m_disp, to_lv_rotation(g_rotation));

    m_card_scene->updateLayout(is_portrait);

    // The sleep panel is hidden but still has to track orientation: it
    // is rendered on the way into deep sleep, with no chance to lay out
    // afterwards.
    m_sleep_panel->updateLayout(is_portrait);

    // The whole frame changes, so invalidate unconditionally.
    lv_obj_invalidate(m_screen_root);
}

// ============================================================
// BATTERY TIMER
//
// The esp_timer callback runs on the timer service task, not on the
// display task, so it must not touch LVGL. It formats the string and
// posts it to the same queue button presses use, keeping every widget
// call on a single thread.
// ============================================================
void DisplayEngine::batteryTimerCb(void *arg)
{
    DisplayEngine *self = (DisplayEngine *)arg;

    uint16_t mv = battery_read_mv();
    if (mv == 0)
        return;

    uint8_t percent = battery_mv_to_percent(mv);

    ESP_LOGI(TAG, "Battery: %u mV -> %u%%", (unsigned)mv, (unsigned)percent);

    // Skip the redraw when nothing changed. A repaint costs a panel
    // refresh and advances the ghost-clear counter, so posting an
    // identical value once a minute would eventually make an idle badge
    // flash on its own.
    //
    // The stored value updates regardless - the main loop reads it for
    // the low-battery check.
    bool changed = (percent != self->m_battery_percent);
    self->m_battery_percent = percent;

    if (!changed)
        return;

    char buf[32];
    snprintf(buf, sizeof(buf), ui().battery_fmt, (unsigned)percent);

    UIEvent cmd;
    cmd.command = CommandType::UPDATE_BATTERY;
    cmd.payload = strdup(buf);
    if (cmd.payload == nullptr)
        return;

    if (xQueueSend(self->m_queue, &cmd, 0) != pdTRUE)
    {
        ESP_LOGW(TAG, "Queue full, battery reading dropped");
        free(cmd.payload);
    }
}

void DisplayEngine::startBatteryTimer()
{
    if (!battery_init())
    {
        ESP_LOGE(TAG, "Battery ADC unavailable, charge indicator will not update");
        return;
    }

    esp_timer_create_args_t args = {};
    args.callback = &DisplayEngine::batteryTimerCb;
    args.arg = this;
    args.dispatch_method = ESP_TIMER_TASK;
    args.name = "battery";

    esp_err_t err = esp_timer_create(&args, &m_battery_timer);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_timer_create failed: %s", esp_err_to_name(err));
        return;
    }

    esp_timer_start_periodic(m_battery_timer, (uint64_t)BATTERY_POLL_INTERVAL_MS * 1000ULL);

    // Take the first reading immediately rather than showing a
    // placeholder for a full minute.
    batteryTimerCb(this);
}

// ============================================================
// SLEEP BUTTON
//
// Polled from the main loop rather than driven by an interrupt: the
// loop already runs at least every 150 ms, which is far finer than the
// multi-second hold this measures.
// ============================================================
void DisplayEngine::checkSleepButton()
{
    bool pressed = power_button_pressed();
    int64_t now = esp_timer_get_time();

    if (!pressed)
    {
        m_button_down_us = 0;
        return;
    }

    if (m_button_down_us == 0)
    {
        m_button_down_us = now;
        ESP_LOGI(TAG, "Sleep button down");
        return;
    }

    if ((now - m_button_down_us) >= (int64_t)SLEEP_BUTTON_HOLD_MS * 1000)
    {
        ESP_LOGI(TAG, "Sleep button held, shutting down");
        enterDeepSleep();
    }
}

// ============================================================
// LOW BATTERY PROTECTION
//
// Idle current alone flattens a cell over weeks of storage, and a
// Li-Po taken to full discharge degrades permanently. Below the
// configured threshold the badge shuts down rather than idling on.
//
// Disabled when BATTERY_CRITICAL_PERCENT is 0.
// ============================================================
void DisplayEngine::checkBatteryLevel()
{
#if BATTERY_CRITICAL_PERCENT > 0
    // 0xFF means no reading has completed yet. Acting on it would shut
    // the badge down at boot on a board with no battery attached.
    if (m_battery_percent == 0xFF)
        return;

    if (m_battery_percent > BATTERY_CRITICAL_PERCENT)
        return;

    ESP_LOGW(TAG, "Battery at %u%% - shutting down to protect the cell",
             (unsigned)m_battery_percent);
    enterDeepSleep();
#endif
}

void DisplayEngine::enterDeepSleep()
{
    // Leaving the idle state first: the sleep screen has to render, and
    // the full-refresh path is skipped while idle.
    m_is_idle = false;

    m_card_scene->setVisible(false);
    m_statusbar->setVisible(false);
    m_sleep_panel->setVisible(true);

    // The sleep screen replaces everything. Without a full refresh,
    // fragments of the card would show through and the badge would look
    // broken rather than switched off.
    graphics_core_request_full_refresh();
    lv_obj_invalidate(m_screen_root);

    // Drive rendering to completion before cutting power. One
    // lv_timer_handler call does not flush the whole invalidated area,
    // and a half-drawn frame is what stays on the panel.
    for (int i = 0; i < 15; i++)
    {
        lv_tick_inc(50);
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    // Do not sleep with the button still down - the wake source is
    // level-triggered and would fire immediately.
    power_wait_button_release();

    power_enter_deep_sleep();
}

void DisplayEngine::loop()
{
    // Holds applied before deep sleep persist across the wake, and
    // would block the touch controller from being detected.
    power_release_panel_pins();

    epd_init();
    epd_poweron();
    epd_poweroff_all();

    power_button_init();

    // Woken by the button: it is still held right now. Waiting here
    // avoids the press being counted as a fresh shutdown request the
    // moment the loop starts polling.
    if (power_woke_from_button())
    {
        ESP_LOGI(TAG, "Woke from deep sleep via button");
        power_wait_button_release();
    }

    initLvgl();
    buildUI();
    startBatteryTimer();

    uint32_t last_tick_time = esp_timer_get_time() / 1000;

    while (true)
    {
        uint32_t current_time = esp_timer_get_time() / 1000;
        lv_tick_inc(current_time - last_tick_time);
        last_tick_time = current_time;

        checkSleepButton();
        checkBatteryLevel();

#if RENDER_MODE == RENDER_MODE_PARTIAL
        // The driver has asked for a full refresh - invalidate the
        // ENTIRE screen before handing control to LVGL.
        //
        // Order is critical. epd_clear() inside epd_flush_cb wipes the
        // whole panel, so the frame it fires on must be a full one.
        // Move this check anywhere after lv_timer_handler, or into
        // applyOrientation, and the panel gets cleared while LVGL
        // redraws only what it considered dirty - the interface
        // vanishes until the next full update.
        //
        // Skipped while asleep: nobody is looking.
        if (!m_is_idle && graphics_core_is_full_refresh_pending())
        {
            lv_obj_invalidate(m_screen_root);
        }
#endif

        uint32_t wait_ms = lv_timer_handler();
        wait_ms = std::max((uint32_t)5, std::min(wait_ms, (uint32_t)100));

        uint32_t inactive_ms = lv_display_get_inactive_time(NULL);
        bool should_idle = (inactive_ms >= SLEEP_IDLE_TIMEOUT_MS);

        if (should_idle && !m_is_idle)
        {
            m_is_idle = true;
            ESP_LOGI(TAG, "Idle - entering sleep loop, polling touch every %d ms", SLEEP_TOUCH_POLL_MS);
        }
        else if (!should_idle && m_is_idle)
        {
            m_is_idle = false;
            ESP_LOGI(TAG, "Touch detected - leaving sleep loop");
        }

        // While asleep the queue is only drained, never waited on:
        // light sleep provides all the waiting.
        TickType_t queue_wait = m_is_idle ? 0 : pdMS_TO_TICKS(wait_ms);

        UIEvent event;
        if (xQueueReceive(m_queue, &event, queue_wait) == pdTRUE)
        {
            switch (event.command)
            {
            case CommandType::LOAD_SCENE_PROFILE:
                if (event.payload)
                    m_card_scene->loadCustomLink(event.payload);
                break;

            case CommandType::CYCLE_ORIENTATION:
                ESP_LOGI(TAG, "Command: CYCLE_ORIENTATION");
                g_rotation = static_cast<DisplayRotation>((static_cast<int>(g_rotation) + 1) % 4);
                applyOrientation();
                break;

            case CommandType::UPDATE_BATTERY:
                // Not repainted while asleep: a redraw costs more than
                // the reading itself and nobody is watching.
                if (event.payload && !m_is_idle)
                    m_statusbar->setBattery(event.payload);
                break;

            case CommandType::FULL_REFRESH:
                ESP_LOGI(TAG, "Command: FULL_REFRESH");
#if RENDER_MODE == RENDER_MODE_PARTIAL
                // The flag is consumed at the top of the next
                // iteration, which is where the full-screen
                // invalidation happens.
                graphics_core_request_full_refresh();
#else
                // In FULL mode there is nothing to accumulate, but the
                // gesture should still do something visible.
                lv_obj_invalidate(m_screen_root);
#endif
                break;
            }
            if (event.payload)
                free(event.payload);
        }

        if (m_is_idle)
        {
            // Yield to the idle task before sleeping. Light sleep is
            // not a scheduler block, so without this DisplayTask
            // (priority 5, core 1) would monopolise its core and starve
            // everything else pinned there.
            vTaskDelay(1);

            // Sleep until the next touch poll. On wake the loop runs
            // again, lv_timer_handler calls lv_touchpad_read, and a
            // finger on the panel resets the inactivity timer by
            // itself.
            power_light_sleep_ms(SLEEP_TOUCH_POLL_MS);
        }
    }
}

void DisplayEngine::startTask(void *arg) { static_cast<DisplayEngine *>(arg)->loop(); }