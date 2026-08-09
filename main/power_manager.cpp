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

void power_button_init()
{
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << SLEEP_BUTTON_PIN);
    io_conf.mode = GPIO_MODE_INPUT;
    // The button pulls to ground; the internal pull-up supplies the
    // idle high level.
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);

    ESP_LOGI(TAG, "Sleep button ready on IO%d", SLEEP_BUTTON_PIN);
}

bool power_button_pressed()
{
    return gpio_get_level((gpio_num_t)SLEEP_BUTTON_PIN) == 0;
}

void power_wait_button_release()
{
    if (!power_button_pressed())
        return;

    ESP_LOGI(TAG, "Waiting for button release...");
    while (power_button_pressed())
    {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    // Mechanical contacts bounce for a few milliseconds after
    // release; sampling too soon reads a phantom press.
    vTaskDelay(pdMS_TO_TICKS(50));
    ESP_LOGI(TAG, "Button released");
}

bool power_woke_from_button()
{
    return esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1;
}

void power_light_sleep_ms(uint32_t duration_ms)
{
    // Timer is the only wake source here.
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
    // nobody can perceive.
    esp_sleep_enable_timer_wakeup((uint64_t)duration_ms * 1000ULL);

    esp_light_sleep_start();

    // Clear the timer source so it cannot be inherited by the deep
    // sleep configured below.
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
}

void power_enter_deep_sleep()
{
    ESP_LOGI(TAG, "Preparing for deep sleep...");

    // The caller is expected to have rendered the sleep screen and
    // let it finish flushing before getting here. The panel retains
    // whatever was last drawn.
    epd_poweroff_all();
    gt911_sleep();
    i2c_driver_delete(I2C_MASTER_NUM);

    // Hold the button high through sleep so a press reliably pulls it
    // low. The regular GPIO pull-up is not retained once the digital
    // domain powers down, hence the RTC variant.
    rtc_gpio_pullup_en((gpio_num_t)SLEEP_BUTTON_PIN);
    rtc_gpio_pulldown_dis((gpio_num_t)SLEEP_BUTTON_PIN);

    esp_sleep_enable_ext1_wakeup((1ULL << SLEEP_BUTTON_PIN), ESP_EXT1_WAKEUP_ANY_LOW);

    ESP_LOGI(TAG, "Entering deep sleep, IO%d wakes", SLEEP_BUTTON_PIN);
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_deep_sleep_start();
}