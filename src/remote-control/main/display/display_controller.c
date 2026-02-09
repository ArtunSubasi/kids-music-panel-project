#include "display_controller.h"
#include <stdbool.h>
#include "esp_event.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "common/config.h"

static const char *TAG = "DISPLAY_CONTROLLER";

static display_t *s_display = NULL;
static bool s_handlers_registered = false;

static void display_wifi_event_handler(void *arg,
									   esp_event_base_t event_base,
									   int32_t event_id,
									   void *event_data)
{
	if (!s_display) {
		return;
	}

	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
		display_show(s_display, DISPLAY_MSG_CONNECTING_WIFI);
		return;
	}

	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
		display_show(s_display, DISPLAY_MSG_WIFI_FAILED);
		return;
	}

	if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		display_show(s_display, DISPLAY_MSG_WIFI_CONNECTED);
		return;
	}
}

esp_err_t display_controller_init(display_t *display)
{
	if (!display) {
		ESP_LOGE(TAG, "Display handle is NULL");
		return ESP_ERR_INVALID_ARG;
	}

	s_display = display;

	if (s_handlers_registered) {
		return ESP_OK;
	}

    /* Register event handlers */
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &display_wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &display_wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

	s_handlers_registered = true;
	ESP_LOGI(TAG, "Display controller initialized");
	return ESP_OK;
}
