#ifndef BUTTONS_H
#define BUTTONS_H

#include "esp_err.h"
#include "esp_event.h"
#include "esp_event_base.h"

#define PIN_PREVIOUS_TRACK GPIO_NUM_22
#define PIN_PLAY_PAUSE GPIO_NUM_21
#define PIN_NEXT_TRACK GPIO_NUM_19

/**
 * @file buttons.h
 * @brief Button handling module
 */
esp_err_t buttons_init();

esp_err_t buttons_register(int pinNumber);

ESP_EVENT_DECLARE_BASE(BUTTON_EVENT);

#endif /* BUTTONS_H */
