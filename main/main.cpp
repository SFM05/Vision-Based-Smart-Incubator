#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "test.hpp"

static const char *TAG = "main";

extern "C" void app_main(void)
{
    iot_test();
}
