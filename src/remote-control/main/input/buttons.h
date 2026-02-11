#ifndef BUTTONS_H
#define BUTTONS_H

#include "esp_err.h"


/**
 * @file buttons.h
 * @brief Button handling module
 */
esp_err_t buttons_init();

esp_err_t buttons_register(int pinNumber);


#endif /* BUTTONS_H */
