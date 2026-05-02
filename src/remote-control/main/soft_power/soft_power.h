#ifndef SOFT_POWER_H
#define SOFT_POWER_H

#include "esp_err.h"

esp_err_t soft_power_init(void);
esp_err_t soft_power_shutdown(void);

#endif /* SOFT_POWER_H */
