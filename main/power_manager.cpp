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
#include "ed047tc1.h"
}

static const char *TAG = "POWER";

// ============================================================
// PANEL LINES
//
// Taken from the driver's own definitions rather than hard-coded
// numbers, so this tracks any upstream pin change.
//
// CFG_STR is deliberately excluded. It is IO0, the strapping pin that
// selects download mode at reset, and holding it low across a wake
// would drop the chip into the bootloader instead of the firmware.
// That leaves part of the shift register unlatched, which is an
// accepted trade-off.
// ============================================================
static const gpio_num_t EPD_PINS[] = {
    D0,
    D1,
    D2,
    D3,
    D4,
    D5,
    D6,
    D7,
    CKV,
    STH,
    CKH,
    CFG_DATA,
    CFG_CLK,
};

static constexpr size_t EPD_PIN_COUNT = sizeof(EPD_PINS) / sizeof(EPD_PINS[0]);

// ============================================================
// Drive the panel lines low and latch them before deep sleep.
//
// With the EPD rail switched off by epd_poweroff_all(), any pin left
// high or floating leaks current through the panel's clamping diodes
// back into the dead rail. This is a documented board-level issue on
// the T5-4.7: upstream measured STH and CKH sitting at roughly 2.2 V
// after poweroff, and it accounts for several milliamps in deep sleep
// where the datasheet promises microamps.
//
// gpio_hold_en latches the level across the sleep transition. Without
// it the digital domain powers down and the pins float again.
//
// This does not eliminate the leak. Whatever the shift register
// drives stays outside SoC control, so some current path remains
// regardless.
// ============================================================
static void isolate_panel_pins()
{
    for (size_t i = 0; i < EPD_PIN_COUNT; i++)
    {
        gpio_num_t pin = EPD_PINS[i];

        gpio_reset_pin(pin);
        gpio_set_direction(pin, GPIO_MODE_OUTPUT);
        gpio_set_level(pin, 0);

        // Pins above IO21 sit outside the RTC domain on the S3 and may
        // refuse to hold. Failures are logged rather than fatal - such
        // a pin simply floats as it did before, and the level set
        // above still helps until the domain powers down.
        esp_err_t err = gpio_hold_en(pin);
        if (err != ESP_OK)
        {
            ESP_LOGW(TAG, "gpio_hold_en failed on IO%d: %s",
                     (int)pin, esp_err_to_name(err));
        }
    }

    // Enables the hold mechanism for the sleep transition itself.
    // Without this the individual holds are released when the digital
    // domain powers down.
    gpio_deep_sleep_hold_en();

    ESP_LOGI(TAG, "Panel pins driven low and latched");
}

void power_release_panel_pins()
{
    // Holds survive the wake transition, so without releasing them the
    // panel driver cannot drive its own data bus and the display stays
    // blank. Safe to call on a cold boot, where no holds exist.
    gpio_deep_sleep_hold_dis();

    for (size_t i = 0; i < EPD_PIN_COUNT; i++)
    {
        gpio_hold_dis(EPD_PINS[i]);
    }

    ESP_LOGI(TAG, "Panel pin latches released");
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
    // or reset itself. Not worth the risk for a latency improvement
    // nobody can perceive - 150 ms disappears next to the hundreds of
    // milliseconds an e-ink redraw takes.
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

    // Order matters: cut the panel's power first, then pull its lines
    // down. The reverse would drive zeros into a live panel.
    isolate_panel_pins();

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