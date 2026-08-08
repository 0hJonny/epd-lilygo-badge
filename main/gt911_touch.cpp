#include "gt911_touch.h"

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "driver/gpio.h"

#include "app_config.h"

static const char *TAG = "GT911";

// Bus scan, kept for bring-up on unfamiliar board revisions.
static void i2c_scanner()
{
    ESP_LOGI(TAG, "=== I2C scan ===");
    int devices_found = 0;
    for (uint8_t i = 1; i < 127; i++)
    {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (i << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(10));
        i2c_cmd_link_delete(cmd);

        if (ret == ESP_OK)
        {
            ESP_LOGI(TAG, "=> device found at 0x%02X", i);
            devices_found++;
        }
    }
    if (devices_found == 0)
    {
        ESP_LOGE(TAG, "=> no I2C devices found, check the SCL/SDA pins");
    }
    ESP_LOGI(TAG, "================");
}

void i2c_bus_init()
{
    ESP_LOGI(TAG, "Initialising INT pin and I2C bus...");

    // The GT911 samples its INT pin at reset to choose between two
    // I2C addresses. Driving it high selects 0x5D.
    gpio_set_direction((gpio_num_t)TOUCH_INT_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)TOUCH_INT_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(20));

    // Release the pin once the address is latched.
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_direction((gpio_num_t)TOUCH_INT_PIN, GPIO_MODE_INPUT);

    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = 400000;

    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);

    // Read the product ID as a definitive check. Should print "911".
    uint8_t reg_id[2] = {0x81, 0x40};
    uint8_t pid[5] = {0};
    esp_err_t err = i2c_master_write_read_device(I2C_MASTER_NUM, GT911_ADDR, reg_id, 2, pid, 4, pdMS_TO_TICKS(50));
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "GT911 product ID: %s", (char *)pid);
    }
    else
    {
        ESP_LOGE(TAG, "Could not read the GT911 product ID");
    }

    i2c_scanner();
}

static bool gt911_read_touch(int16_t &x, int16_t &y)
{
    uint8_t status = 0;
    uint8_t reg[2] = {0x81, 0x4E};

    esp_err_t err = i2c_master_write_read_device(I2C_MASTER_NUM, GT911_ADDR, reg, 2, &status, 1, pdMS_TO_TICKS(10));
    if (err != ESP_OK)
        return false;

    // Bit 7 means the data buffer is ready.
    if ((status & 0x80) == 0)
    {
        return false;
    }

    // Low four bits hold the number of touch points.
    uint8_t point_num = status & 0x0F;
    bool is_pressed = false;

    if (point_num > 0)
    {
        // First touch point lives at registers 0x8150-0x8153,
        // little-endian coordinates.
        uint8_t point_reg[2] = {0x81, 0x50};
        uint8_t data[4];
        err = i2c_master_write_read_device(I2C_MASTER_NUM, GT911_ADDR, point_reg, 2, data, 4, pdMS_TO_TICKS(10));

        if (err == ESP_OK)
        {
            x = data[0] | (data[1] << 8);
            y = data[2] | (data[3] << 8);
            is_pressed = true;
        }
    }

    // CRITICAL: clear the buffer-ready flag unconditionally.
    // Skipping this on release, when point_num is zero, leaves the
    // flag set and the controller stops reporting new touches
    // entirely - the screen appears to freeze.
    uint8_t clr_cmd[3] = {0x81, 0x4E, 0x00};
    i2c_master_write_to_device(I2C_MASTER_NUM, GT911_ADDR, clr_cmd, 3, pdMS_TO_TICKS(10));

    return is_pressed;
}

void gt911_sleep()
{
    ESP_LOGI(TAG, "Sending sleep command to the GT911...");
    uint8_t sleep_cmd[3] = {0x80, 0x40, 0x05};
    i2c_master_write_to_device(I2C_MASTER_NUM, GT911_ADDR, sleep_cmd, 3, pdMS_TO_TICKS(50));
}

// Fixed correction for how the touch film is bonded to the panel:
// on the T5-4.7 the two are rotated 90 degrees relative to each
// other, so raw GT911 axes have to be mapped onto physical panel
// axes.
//
// This is NOT screen-rotation handling. LVGL 9 transforms coordinates
// itself inside lv_display_set_rotation(), and this correction is
// required in every orientation including ROT_0. Adding a second
// transform on top of it breaks all four orientations.
//
// The chain is:
//   GT911 -> this correction -> physical panel -> LVGL rotation
//   -> logical coordinates
void lv_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    // Coordinates persist across calls: LVGL expects the last known
    // position to remain valid while the state is RELEASED.
    static int16_t last_x = 0;
    static int16_t last_y = 0;
    static bool last_pressed = false;
    int16_t raw_x, raw_y;

    bool is_pressed = gt911_read_touch(raw_x, raw_y);

    if (is_pressed)
    {
        data->state = LV_INDEV_STATE_PRESSED;

        int16_t phys_x = raw_y;
        int16_t phys_y = (PHYS_H - 1) - raw_x;

        last_x = phys_x;
        last_y = phys_y;

        // Logged on the transition only - logging every poll would
        // flood the console at the touch sampling rate.
        if (!last_pressed)
        {
            ESP_LOGI(TAG, "Touch PRESSED: raw GT911(x:%d, y:%d) -> LVGL(x:%d, y:%d)", raw_x, raw_y, last_x, last_y);
            last_pressed = true;
        }
    }
    else
    {
        data->state = LV_INDEV_STATE_RELEASED;
        if (last_pressed)
        {
            ESP_LOGI(TAG, "Touch RELEASED");
            last_pressed = false;
        }
    }
    data->point.x = last_x;
    data->point.y = last_y;
}