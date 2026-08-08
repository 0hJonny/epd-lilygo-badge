#include "power_manager.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"

#include "app_config.h"
#include "gt911_touch.h"

extern "C"
{
#include "epd_driver.h"
}

static const char *TAG = "POWER";

void power_light_sleep_ms(uint32_t duration_ms)
{
    // Timer is the only wake source.
    //
    // Why not GPIO47, the GT911 interrupt line: GPIO wakeup from
    // light sleep is level-triggered only, and the GT911 emits a
    // brief pulse on INT in its default configuration - the pulse
    // would fall between polling windows and be missed.
    //
    // Switching INT to level-hold mode means writing register 0x804D,
    // which sits inside a checksum-protected configuration block: a
    // naive single-byte write makes the controller reject the config
    // or reset itself. Not worth the risk for a latency improvement
    // nobody can perceive - a 150 ms poll interval disappears next to
    // the hundreds of milliseconds an e-ink redraw takes.
    esp_sleep_enable_timer_wakeup((uint64_t)duration_ms * 1000ULL);

    esp_light_sleep_start();

    // Clear the timer source so it cannot be inherited by an
    // unrelated sleep later on.
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
}

void power_enter_deep_sleep()
{
    ESP_LOGI(TAG, "Preparing for deep sleep...");
    epd_poweroff_all();
    gt911_sleep();
    i2c_driver_delete(I2C_MASTER_NUM);

    // Pull up so the pin idles high and goes cleanly low when the
    // button is pressed.
    rtc_gpio_pullup_en((gpio_num_t)WAKEUP_BUTTON_PIN);
    rtc_gpio_pulldown_dis((gpio_num_t)WAKEUP_BUTTON_PIN);

    // Wake on a low level at WAKEUP_BUTTON_PIN.
    esp_sleep_enable_ext1_wakeup((1ULL << WAKEUP_BUTTON_PIN), ESP_EXT1_WAKEUP_ANY_LOW);

    ESP_LOGI(TAG, "Entering deep sleep");
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_deep_sleep_start();
}