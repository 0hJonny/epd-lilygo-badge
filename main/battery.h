#pragma once

#include <stdint.h>
#include <stdbool.h>

// ==========================================================
// BATTERY VOLTAGE MEASUREMENT (ADC2 + esp_adc_cal)
//
// This module intentionally knows nothing about the UI or FreeRTOS queues:
// it only performs measurements. Who calls it and when is up to the caller.
//
// CHARGING NOTE: It is impossible to detect a USB connection using this
// ADC. It is connected to the battery circuit AFTER the charge
// controller, and when powered via USB, the controller maintains a
// float charge voltage (~4.2 V) there — exactly the same as what a
// fully charged cell provides. It is fundamentally impossible
// to distinguish between the two, so there is no "charging" indication here.
// ==========================================================

// One-time ADC initialization and calibration loading.
// Returns false if the channel configuration fails.
bool battery_init();

// Cell voltage in millivolts (already accounting for the voltage divider).
// Returns 0 if the measurement fails.
uint16_t battery_read_mv();

// Rough capacity estimation in percent (0..100) based on the Li-Po discharge curve.
// mv — the result of battery_read_mv().
uint8_t battery_mv_to_percent(uint16_t mv);