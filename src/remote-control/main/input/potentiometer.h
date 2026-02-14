#ifndef POTENTIOMETER_H
#define POTENTIOMETER_H

#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"

/**
 * @file potentiometer.h
 * @brief Potentiometer volume control module
 * 
 * Reads analog voltage from a B10K linear potentiometer via ADC1
 * and converts it to a volume level (0-100%). Uses smoothing and
 * hysteresis to prevent jitter from analog noise.
 */

/* Volume range constants */
#define VOLUME_MIN 0
#define VOLUME_MAX 100

/* ADC configuration */
#define ADC_RESOLUTION_BITS 12
#define ADC_MAX_VALUE ((1 << ADC_RESOLUTION_BITS) - 1)  /* 4095 */

/* Smoothing and hysteresis */
#define POTENTIOMETER_MOVING_AVG_SIZE 8
#define POTENTIOMETER_HYSTERESIS_PERCENT 2  /* Only log if change > 2% */

/* Sampling rate */
#define POTENTIOMETER_SAMPLE_INTERVAL_MS 100

/**
 * @brief Initialize the potentiometer module
 * 
 * Sets up ADC1 with 12-bit resolution and 11dB attenuation,
 * creates the ADC reading task that polls every 100ms.
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t potentiometer_init(void);

/**
 * @brief Get the current volume level
 * 
 * @return Current volume level (0-100)
 */
int potentiometer_get_volume(void);

#endif /* POTENTIOMETER_H */
