#include "esp_wifi.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "nvs_flash.h"


void wifi_init_sta();
void http_get_task(void *pvParameters);
