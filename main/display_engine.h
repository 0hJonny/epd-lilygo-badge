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
// the firmware happens on this task; other contexts post commands
// to m_queue instead.
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

    esp_timer_handle_t m_battery_timer;

    // True while in the power-saving loop: short light sleeps
    // interleaved with touch polling.
    bool m_is_idle;

    void initLvgl();
    void buildUI();
    void applyOrientation();

    void startBatteryTimer();
    static void batteryTimerCb(void *arg);

public:
    DisplayEngine(QueueHandle_t q);

    void loop();
    static void startTask(void *arg);
};