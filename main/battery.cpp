#include "battery.h"

#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_log.h"

#include "app_config.h"

static const char *TAG = "BATTERY";

// Roughly 11/12 dB attenuation, full scale about 3.1 V at the pin.
//
// Given as a raw number rather than a named constant: depending on
// the ESP-IDF release this field is called ADC_ATTEN_DB_11 or
// ADC_ATTEN_DB_12 (identical values), and either name produces a
// -Wdeprecated-declarations warning on one of the branches. The
// numeric value is fixed in hardware and never changes.
static constexpr adc_atten_t BATTERY_ADC_ATTEN = (adc_atten_t)3;

static esp_adc_cal_characteristics_t s_adc_chars;
static bool s_initialized = false;

// Li-Po discharge curve under light load, linearly interpolated
// between points. More honest than a straight line from 3.3 to 4.2 V:
// a real cell spends most of its life around 3.7-3.9 V, so a linear
// mapping would report a near-empty battery for hours.
struct BatteryCurvePoint
{
    uint16_t mv;
    uint8_t percent;
};

static const BatteryCurvePoint BATTERY_CURVE[] = {
    {4200, 100},
    {4100, 90},
    {4000, 80},
    {3950, 70},
    {3870, 60},
    {3820, 50},
    {3790, 40},
    {3750, 30},
    {3700, 20},
    {3600, 10},
    {3300, 0},
};
static constexpr int BATTERY_CURVE_COUNT = sizeof(BATTERY_CURVE) / sizeof(BATTERY_CURVE[0]);

bool battery_init()
{
    if (s_initialized)
        return true;

    esp_err_t err = adc2_config_channel_atten((adc2_channel_t)BATTERY_ADC_CHANNEL, BATTERY_ADC_ATTEN);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "adc2_config_channel_atten failed: %s", esp_err_to_name(err));
        return false;
    }

    esp_adc_cal_value_t cal_type = esp_adc_cal_characterize(
        ADC_UNIT_2, BATTERY_ADC_ATTEN, ADC_WIDTH_BIT_12, 0, &s_adc_chars);

    switch (cal_type)
    {
    case ESP_ADC_CAL_VAL_EFUSE_TP:
        ESP_LOGI(TAG, "ADC calibration: eFuse two point");
        break;
    case ESP_ADC_CAL_VAL_EFUSE_VREF:
        ESP_LOGI(TAG, "ADC calibration: eFuse Vref");
        break;
    case ESP_ADC_CAL_VAL_EFUSE_TP_FIT:
        ESP_LOGI(TAG, "ADC calibration: eFuse curve fitting");
        break;
    default:
        ESP_LOGW(TAG, "No factory ADC calibration, falling back to default Vref - readings will be less accurate");
        break;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "Battery ADC ready on GPIO%d", BATTERY_ADC_GPIO);
    return true;
}

uint16_t battery_read_mv()
{
    if (!s_initialized)
    {
        ESP_LOGW(TAG, "battery_read_mv called before battery_init");
        return 0;
    }

    // Averaging several samples suppresses ADC noise, which is
    // significant on ADC2.
    uint32_t raw_accum = 0;
    int good_samples = 0;

    for (int i = 0; i < BATTERY_ADC_SAMPLES; i++)
    {
        int raw = 0;
        esp_err_t err = adc2_get_raw((adc2_channel_t)BATTERY_ADC_CHANNEL, ADC_WIDTH_BIT_12, &raw);
        if (err == ESP_OK)
        {
            raw_accum += (uint32_t)raw;
            good_samples++;
        }
    }

    if (good_samples == 0)
    {
        ESP_LOGE(TAG, "adc2_get_raw returned no usable samples");
        return 0;
    }

    uint32_t raw_avg = raw_accum / (uint32_t)good_samples;
    uint32_t mv_on_pin = esp_adc_cal_raw_to_voltage(raw_avg, &s_adc_chars);

    // The board halves the battery voltage before the ADC.
    uint32_t mv_battery = mv_on_pin * BATTERY_DIVIDER_RATIO;

    ESP_LOGD(TAG, "raw=%lu -> pin %lu mV -> cell %lu mV",
             (unsigned long)raw_avg, (unsigned long)mv_on_pin, (unsigned long)mv_battery);

    return (uint16_t)mv_battery;
}

uint8_t battery_mv_to_percent(uint16_t mv)
{
    if (mv == 0)
        return 0;
    if (mv >= BATTERY_CURVE[0].mv)
        return 100;
    if (mv <= BATTERY_CURVE[BATTERY_CURVE_COUNT - 1].mv)
        return 0;

    for (int i = 0; i < BATTERY_CURVE_COUNT - 1; i++)
    {
        uint16_t hi_mv = BATTERY_CURVE[i].mv;
        uint16_t lo_mv = BATTERY_CURVE[i + 1].mv;

        if (mv <= hi_mv && mv > lo_mv)
        {
            uint8_t hi_pct = BATTERY_CURVE[i].percent;
            uint8_t lo_pct = BATTERY_CURVE[i + 1].percent;

            // Integer interpolation - no floating point needed for
            // one decimal place of precision we do not display.
            uint32_t span_mv = (uint32_t)(hi_mv - lo_mv);
            uint32_t span_pct = (uint32_t)(hi_pct - lo_pct);
            uint32_t offset_mv = (uint32_t)(mv - lo_mv);

            return (uint8_t)(lo_pct + (offset_mv * span_pct) / span_mv);
        }
    }
    return 0;
}