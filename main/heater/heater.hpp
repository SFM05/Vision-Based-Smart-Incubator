#pragma once

#include "esp_err.h"

esp_err_t heater_init(void);
esp_err_t heater_start_pid(void);
float heater_get_power(void);
