#include "power_manager.h"

#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "driver/periph_ctrl.h"
#include "esp_rom_uart.h"

#include "app_config.h"
#include "gt911_touch.h"

extern "C"
{
#include "epd_driver.h"
}

static const char *TAG = "POWER";

void power_release_panel_pins()
{
    // The touch interrupt line is latched low before deep sleep to keep
    // the controller asleep. It has to be released before
    // i2c_bus_init(), which drives INT high to select the 0x5D address -
    // a latched pin would leave the touch undetected.
    //
    // Safe on a cold boot, where no hold exists.
    gpio_deep_sleep_hold_dis();
    gpio_hold_dis((gpio_num_t)TOUCH_INT_PIN);

    ESP_LOGI(TAG, "Pin latches released");
}

// ============================================================
// Shut down the debug console before deep sleep.
//
// The vendor demo calls Serial.end() at this point and reports 388 uA
// for the board. ESP-IDF has no direct equivalent for the USB
// Serial/JTAG console, so this drains the output and gates the
// peripheral's clock instead.
//
// Nothing can be logged after this returns.
// ============================================================
static void shutdown_console()
{
    // Let anything still queued reach the host before the peripheral
    // goes away, otherwise the last lines are lost.
    fflush(stdout);
    esp_rom_uart_tx_wait_idle(0);
    vTaskDelay(pdMS_TO_TICKS(50));

#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    periph_module_disable(PERIPH_USB_MODULE);
#endif
}

void power_button_init()
{
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << SLEEP_BUTTON_PIN);
    io_conf.mode = GPIO_MODE_INPUT;
    // The button pulls to ground; the internal pull-up supplies the
    // idle high level. Verified active-low on rev V2.4.
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
    // Mechanical contacts bounce for a few milliseconds after release;
    // sampling too soon reads a phantom press.
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
    // Why not GPIO47, the GT911 interrupt line: GPIO wakeup from light
    // sleep is level-triggered only, and the GT911 emits a brief pulse
    // on INT in its default configuration - the pulse would fall
    // between polling windows and be missed.
    //
    // Switching INT to level-hold mode means writing register 0x804D,
    // which sits inside a checksum-protected configuration block: a
    // naive single-byte write makes the controller reject the config
    // or reset itself.
    //
    // Note the touch controller stays awake throughout this mode by
    // design - it is polled every 150 ms. That is the dominant term in
    // light sleep current, not the CPU.
    esp_sleep_enable_timer_wakeup((uint64_t)duration_ms * 1000ULL);

    esp_light_sleep_start();

    // Clear the timer source so it cannot be inherited by the deep
    // sleep configured below.
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
}

void power_enter_deep_sleep()
{
    ESP_LOGI(TAG, "Preparing for deep sleep...");

    // The caller is expected to have rendered the sleep screen and let
    // it finish flushing before getting here. The panel retains
    // whatever was last drawn.
    epd_poweroff_all();

    // Leaves INT driven low, which is what keeps the controller down.
    gt911_sleep();

    i2c_driver_delete(I2C_MASTER_NUM);

    // Latch INT low across the sleep transition. Once the digital
    // domain powers down the pin floats, and a high level is exactly
    // what brings the touch controller back out of sleep. Without this
    // the controller resumes and draws milliamps for the whole sleep -
    // which is what made deep sleep no better than light sleep before
    // the fix.
    esp_err_t hold_err = gpio_hold_en((gpio_num_t)TOUCH_INT_PIN);
    if (hold_err != ESP_OK)
    {
        ESP_LOGW(TAG, "gpio_hold_en failed on TOUCH_INT: %s",
                 esp_err_to_name(hold_err));
    }
    gpio_deep_sleep_hold_en();

    // Hold the button high through sleep so a press reliably pulls it
    // low. The regular GPIO pull-up is not retained once the digital
    // domain powers down, hence the RTC variant.
    rtc_gpio_pullup_en((gpio_num_t)SLEEP_BUTTON_PIN);
    rtc_gpio_pulldown_dis((gpio_num_t)SLEEP_BUTTON_PIN);

    esp_sleep_enable_ext1_wakeup((1ULL << SLEEP_BUTTON_PIN), ESP_EXT1_WAKEUP_ANY_LOW);

    ESP_LOGI(TAG, "Entering deep sleep, IO%d wakes", SLEEP_BUTTON_PIN);

    // Last thing before sleeping: nothing is logged after this.
    shutdown_console();

    esp_deep_sleep_start();
}