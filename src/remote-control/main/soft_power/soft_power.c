#include "soft_power.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "common/board_pins.h"

static const char *TAG = "SOFT_POWER";

esp_err_t soft_power_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BOARD_SOFT_POWER_OFF),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&io_conf));
    ESP_ERROR_CHECK(gpio_set_level(BOARD_SOFT_POWER_OFF, 0));
    ESP_LOGI(TAG, "Soft power off pin GPIO %d initialized low", BOARD_SOFT_POWER_OFF);

    return ESP_OK;
}

esp_err_t soft_power_shutdown(void)
{
    ESP_LOGI(TAG, "Requesting soft power off by asserting GPIO %d high", BOARD_SOFT_POWER_OFF);
    ESP_ERROR_CHECK(gpio_set_level(BOARD_SOFT_POWER_OFF, 1));
    return ESP_OK;
}
