#include <stdio.h>

#include "esp_log.h"
#include "nvs_manager.h"

static const char *TAG = "APP_MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "System init");
    nvs_manager_init();
}
