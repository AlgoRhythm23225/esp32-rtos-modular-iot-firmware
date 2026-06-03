#include <stdio.h>
#include "wifi_manager.h"
#include "http_client.h"

void app_main(void)
{
    wifi_init_sta();
    
    http_test_task();
}

