#ifndef BUTTONS_H
#define BUTTONS_H

#include "esp_err.h"
#include "esp_event.h"
#include "esp_event_base.h"
#include "driver/gpio.h"

#define PIN_PREVIOUS_TRACK GPIO_NUM_22
#define PIN_PLAY_PAUSE GPIO_NUM_21
#define PIN_NEXT_TRACK GPIO_NUM_19

ESP_EVENT_DECLARE_BASE(BUTTON_EVENT);

typedef enum {
	BUTTON_EVENT_ID_PREVIOUS_TRACK_PRESSED = 1,
	BUTTON_EVENT_ID_PLAY_PAUSE_PRESSED = 2,
	BUTTON_EVENT_ID_NEXT_TRACK_PRESSED = 3,
} buttons_event_id_t;

typedef struct {
	int pin;
	buttons_event_id_t button_id;
} buttons_event_data_t;

/**
 * @file buttons.h
 * @brief Button handling module
 */
esp_err_t buttons_init(void);

esp_err_t buttons_register(int pinNumber);

esp_err_t buttons_subscribe(int32_t event_id, esp_event_handler_t handler, void *handler_arg);

#endif /* BUTTONS_H */
