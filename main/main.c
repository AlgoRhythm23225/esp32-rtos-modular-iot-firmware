#include <stdio.h>
#include "i2c_manager.h"
#include "bh1750.h"
#include "bmp280.h"
#include "aht20.h"
#include "ina226.h"
#include "bno055.h"
#include "i2c_scan.h"

void test_app_main() {
    i2c_init();

    xTaskCreate(bno055_ndof_task, "bno055_test", 2048, NULL, 5, NULL);

    // xTaskCreate(bh1750_app_test, "bh1750_test", 2048, NULL, 5, NULL);
    // xTaskCreate(bmp280_app_test, "bmp280_test", 2048, NULL, 5, NULL);
    // xTaskCreate(aht20_app_test, "aht20_test", 2048, NULL, 5, NULL);
    // xTaskCreate(ina226_app_test, "ina226_test", 2048, NULL, 5, NULL);
    
}

void app_main(void)
{
    // i2c_scan();
    test_app_main();
}