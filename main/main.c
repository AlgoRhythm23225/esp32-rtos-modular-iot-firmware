#include <stdio.h>
#include "i2c_manager.h"
#include "bh1750.h"
#include "bmp280.h"
#include "aht20.h"

void app_main(void)
{
    i2c_init();
    
    xTaskCreate(bh1750_app_test, "bh1750_test", 2048, NULL, 5, NULL);
    xTaskCreate(bmp280_app_test, "bmp280_test", 2048, NULL, 5, NULL);
    xTaskCreate(aht20_app_test, "aht20_test", 2048, NULL, 5, NULL);
}