#include <stdio.h>
#include "esp_log.h"
#include "i2c_manager.h"
#include "bh1750.h"
#include "bme280.h"
#include "bmp280.h"

void app_main(void)
{
    i2c_init();

    xTaskCreate(bmp280_app_test, "bmp280_task", 2048, NULL, 5, NULL);
    xTaskCreate(bh1750_app_test, "bh1750_task", 2048, NULL, 5, NULL);
}

