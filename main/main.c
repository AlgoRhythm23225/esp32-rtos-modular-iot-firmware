#include <stdio.h>
#include "esp_log.h"
#include "wifi_manager.h"

void app_main(void)
{
    wifi_init_sta();

    xTaskCreate(http_get_task, "http_get_task", 8192, NULL, 5, NULL);
}

