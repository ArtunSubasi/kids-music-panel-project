#ifndef BUTTONS_H
#define BUTTONS_H

#include "esp_err.h"
#include "esp_event.h"
#include "esp_event_base.h"


/**
 * @file buttons.h
 * @brief Button handling module
 */
esp_err_t buttons_init();

esp_err_t buttons_register(int pinNumber);

ESP_EVENT_DECLARE_BASE(BUTTON_EVENT);

#endif /* BUTTONS_H */
