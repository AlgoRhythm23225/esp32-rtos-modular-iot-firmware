#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "rtos_config.h"
#include "mqtt_task.h"
#include "wifi_task.h"
#include "esp_log.h"
#include "ble_manager.h"
#include "sensors_manager.h"
#include "i2c_scan.h"
#include "storage_service.h"

static const char *TAG = "MAIN";

EventGroupHandle_t g_system_event_group = NULL;
QueueHandle_t      g_mqtt_publish_queue = NULL;
QueueHandle_t      g_control_queue      = NULL;
SemaphoreHandle_t  g_mqtt_ready_sem     = NULL;
QueueHandle_t      g_mqtt_subscribe_queue = NULL;

static esp_err_t rtos_resources_create(void)
{
    g_system_event_group = xEventGroupCreate();
    if (g_system_event_group == NULL) return ESP_ERR_NO_MEM;

    g_mqtt_publish_queue = xQueueCreate(MQTT_PUBLISH_QUEUE_SIZE, sizeof(mqtt_publish_msg_t));
    if (g_mqtt_publish_queue == NULL) return ESP_ERR_NO_MEM;

    g_control_queue = xQueueCreate(10, sizeof(control_cmd_t));
    if (g_control_queue == NULL) return ESP_ERR_NO_MEM;

    g_mqtt_ready_sem = xSemaphoreCreateBinary();
    if (g_mqtt_ready_sem == NULL) return ESP_ERR_NO_MEM;

    g_mqtt_subscribe_queue = xQueueCreate(MQTT_SUBSCRIBE_QUEUE_SIZE, sizeof(mqtt_subscribe_msg_t));
    if (g_mqtt_subscribe_queue == NULL) return ESP_ERR_NO_MEM;
    
    ESP_LOGI(TAG, "RTOS resources created");
    return ESP_OK;
}

void app_main(void)
{
    ble_manager_init();

    esp_err_t ret = rtos_resources_create();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create RTOS resources: %d", ret);
        return;
    }

    ESP_ERROR_CHECK(storage_service_init());
    
    ret = wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi: %d", ret);
        return;
    }

    char ssid[65] = {0};
    char password[65] = {0};
    if (storage_service_read_wifi(ssid, sizeof(ssid), password, sizeof(password)))
    {
        wifi_service_sta_connect(ssid, password);
    }

    xTaskCreate(mqtt_task,
                "mqtt",
                TASK_STACK_MQTT,
                NULL,
                TASK_PRIO_MQTT,
                NULL);
}
