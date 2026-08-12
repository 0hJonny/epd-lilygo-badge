#pragma once

#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_timer.h"
#include "lvgl.h"

#include "app_types.h"
#include "widgets.h"
#include "panels.h"

// ============================================================
// DISPLAY ENGINE
//
// Owns LVGL, the widget tree, and the main loop. Every LVGL call in
// the firmware happens on this task; other contexts post commands to
// m_queue instead.
// ============================================================
class DisplayEngine
{
private:
    QueueHandle_t m_queue;
    lv_display_t *m_disp;
    lv_indev_t *m_indev;
    UIContext m_ctx;

    lv_obj_t *m_screen_root;
    StatusBar *m_statusbar;
    CardScene *m_card_scene;
    SleepPanel *m_sleep_panel;

    esp_timer_handle_t m_battery_timer;

    // Last reported charge level, or 0xFF before the first reading.
    // Held here rather than as a static inside the timer callback so
    // the main loop can act on it.
    uint8_t m_battery_percent;

    // True while in the power-saving loop: short light sleeps
    // interleaved with touch polling.
    bool m_is_idle;

    // Timestamp of the moment the sleep button went down, or 0 while
    // it is up. Used to measure the hold duration.
    int64_t m_button_down_us;

    void initLvgl();
    void buildUI();
    void applyOrientation();

    void startBatteryTimer();
    static void batteryTimerCb(void *arg);

    // Poll the sleep button and trigger shutdown once held long enough.
    void checkSleepButton();

    // Force deep sleep when the charge drops to a critical level.
    void checkBatteryLevel();

    // Render the sleep screen, wait for the flush to finish, then power
    // down. Does not return.
    void enterDeepSleep();

public:
    DisplayEngine(QueueHandle_t q);

    void loop();
    static void startTask(void *arg);
};