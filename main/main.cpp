#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#include "app_config.h"
#include "app_types.h"
#include "display_engine.h"

static const char *TAG = "LILYGO_UI";

// ============================================================
// ENTRY POINT
//
// A single task does everything. There used to be a second task
// polling for battery updates every 15 seconds while doing nothing
// else; it cost 8 KB of stack and a context switch per wake, and was
// replaced by an esp_timer inside DisplayEngine.
// ============================================================
extern "C" void app_main(void)
{
    // Wildcard rather than a single tag: every module has its own
    // TAG, and setting only this one would leave the rest at the
    // Kconfig default.
    esp_log_level_set("*", ESP_LOG_INFO);

    // The e-paper power rails need a moment to settle after reset
    // before epd_init() touches them.
    vTaskDelay(pdMS_TO_TICKS(2000));
    ESP_LOGI(TAG, "Starting up...");

    QueueHandle_t systemQueue = xQueueCreate(DISPLAY_QUEUE_LEN, sizeof(UIEvent));
    if (systemQueue == nullptr)
    {
        ESP_LOGE(TAG, "Failed to create the system queue");
        abort();
    }

    // Static storage: the object has to outlive app_main, which
    // returns while the task keeps running.
    static DisplayEngine displayEngine(systemQueue);

    // Pinned to core 1, away from the system tasks on core 0.
    xTaskCreatePinnedToCore(DisplayEngine::startTask, "DisplayTask", DISPLAY_STACK_SIZE, &displayEngine, 5, NULL, 1);
}