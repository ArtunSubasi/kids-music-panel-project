#include "potentiometer.h"
#include "board_pins.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include <string.h>

static const char *TAG = "POTENTIOMETER";

/* ADC handle */
static adc_oneshot_unit_handle_t s_adc_handle = NULL;

/* Moving average filter buffer */
static int s_adc_samples[POTENTIOMETER_MOVING_AVG_SIZE];
static int s_sample_index = 0;
static bool s_buffer_filled = false;

/* Last logged volume to implement hysteresis */
static int s_last_logged_volume = -1;

/* Current volume level */
static int s_current_volume = 0;

/* Task handle */
static TaskHandle_t s_potentiometer_task_handle = NULL;

/**
 * @brief Add a new ADC sample to the moving average filter
 * 
 * @param raw_value Raw ADC reading (0-4095)
 * @return Smoothed average value
 */
static int moving_average_filter(int raw_value) {
    s_adc_samples[s_sample_index] = raw_value;
    s_sample_index = (s_sample_index + 1) % POTENTIOMETER_MOVING_AVG_SIZE;
    
    if (s_sample_index == 0) {
        s_buffer_filled = true;
    }
    
    /* Calculate average */
    int sum = 0;
    int count = s_buffer_filled ? POTENTIOMETER_MOVING_AVG_SIZE : s_sample_index;
    
    for (int i = 0; i < count; i++) {
        sum += s_adc_samples[i];
    }
    
    return count > 0 ? (sum / count) : raw_value;
}

/**
 * @brief Map ADC value (0-4095) to volume percentage (0-100)
 * 
 * @param adc_value Smoothed ADC reading
 * @return Volume level (0-100)
 */
static int map_adc_to_volume(int adc_value) {
    /* Linear mapping: 0-4095 -> 0-100 */
    int volume = (adc_value * 100) / ADC_MAX_VALUE;
    
    /* Clamp to valid range */
    if (volume < VOLUME_MIN) volume = VOLUME_MIN;
    if (volume > VOLUME_MAX) volume = VOLUME_MAX;
    
    return volume;
}

/**
 * @brief Potentiometer reading task
 * 
 * Continuously reads ADC, applies filtering, and logs volume changes
 */
static void potentiometer_task(void *pvParameters) {
    ESP_LOGI(TAG, "Potentiometer task started");
    
    while (1) {
        int raw_adc = 0;
        esp_err_t err = adc_oneshot_read(s_adc_handle, BOARD_POTENTIOMETER_ADC_CHANNEL, &raw_adc);
        
        if (err == ESP_OK) {
            /* Apply moving average filter */
            int smoothed_adc = moving_average_filter(raw_adc);
            
            /* Map to volume percentage */
            int volume = map_adc_to_volume(smoothed_adc);
            s_current_volume = volume;
            
            /* Check hysteresis - only log if change > 2% */
            if (s_last_logged_volume == -1 || 
                abs(volume - s_last_logged_volume) > POTENTIOMETER_HYSTERESIS_PERCENT) {
                
                ESP_LOGI(TAG, "Volume changed: raw=%d, smoothed=%d, volume=%d%%", 
                         raw_adc, smoothed_adc, volume);
                
                s_last_logged_volume = volume;
            }
        } else {
            ESP_LOGW(TAG, "ADC read failed: %s", esp_err_to_name(err));
        }
        
        /* Wait before next sample */
        vTaskDelay(pdMS_TO_TICKS(POTENTIOMETER_SAMPLE_INTERVAL_MS));
    }
}

esp_err_t potentiometer_init(void) {
    ESP_LOGI(TAG, "Initializing potentiometer on GPIO %d (ADC1_CH%d)", 
             BOARD_POTENTIOMETER_GPIO, BOARD_POTENTIOMETER_ADC_CHANNEL);
    
    /* Initialize moving average buffer */
    memset(s_adc_samples, 0, sizeof(s_adc_samples));
    
    /* Configure ADC1 */
    adc_oneshot_unit_init_cfg_t adc_config = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    
    esp_err_t err = adc_oneshot_new_unit(&adc_config, &s_adc_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ADC unit: %s", esp_err_to_name(err));
        return err;
    }
    
    /* Configure ADC channel */
    adc_oneshot_chan_cfg_t chan_config = {
        .atten = ADC_ATTEN_DB_12,      /* 0-3.3V range (previously DB_11) */
        .bitwidth = ADC_BITWIDTH_12,   /* 12-bit resolution (0-4095) */
    };
    
    err = adc_oneshot_config_channel(s_adc_handle, BOARD_POTENTIOMETER_ADC_CHANNEL, &chan_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ADC channel: %s", esp_err_to_name(err));
        return err;
    }
    
    /* Create ADC reading task */
    BaseType_t ret = xTaskCreate(
        potentiometer_task,
        "pot_task",
        4096,                          /* Stack size */
        NULL,
        5,                             /* Priority */
        &s_potentiometer_task_handle
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create potentiometer task");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Potentiometer initialized successfully");
    return ESP_OK;
}

int potentiometer_get_volume(void) {
    return s_current_volume;
}
