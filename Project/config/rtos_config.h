#ifndef RTOS_RESOURCES_H
#define RTOS_RESOURCES_H

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define TASK_STACK_MQTT         4096
#define TASK_PRIO_MQTT          5

#define MQTT_NETWORK_BUF_SIZE   2048
#define MQTT_KEEP_ALIVE_SEC     60
#define MQTT_CONNECT_TIMEOUT_MS 10000
#define MQTT_RECONNECT_DELAY_MS 5000
#define MQTT_PROCESS_LOOP_MS    100
#define MQTT_PUBLISH_QUEUE_SIZE 10

#define AWS_ENDPOINT  "adiub9wc49ljw-ats.iot.ap-southeast-1.amazonaws.com"
#define AWS_PORT      "8883"
#define AWS_DEVICE_ID "esp32-001"
#define TOPIC_CMD     "devices/" AWS_DEVICE_ID "/ota/command"
#define TOPIC_OTA_CMD "devices/" AWS_DEVICE_ID "/ota/status"

#define OTA_URL_MAX_LEN     1024
#define OTA_VERSION_MAX_LEN 48

#define LED_MQTT_GPIO 2
#define EVT_WIFI_CONNECTED  (1 << 0)
#define EVT_MQTT_CONNECTED  (1 << 1)
#define EVT_OTA_IN_PROGRESS (1 << 2)

typedef enum {
    CMD_UNKNOWN = 0,
    CMD_GPIO_SET,
    CMD_GPIO_RESET,
    CMD_REBOOT,
} cmd_type_t;

typedef struct {
    cmd_type_t cmd;
    uint8_t    gpio_pin;
    uint32_t   duration_ms;
} control_cmd_t;

typedef struct {
    char   topic[128];
    char   payload[512];
    size_t payload_len;
} mqtt_publish_msg_t;

extern EventGroupHandle_t g_system_event_group;
extern QueueHandle_t      g_mqtt_publish_queue;
extern QueueHandle_t      g_control_queue;
extern SemaphoreHandle_t  g_mqtt_ready_sem;

extern char g_ota_url[OTA_URL_MAX_LEN];
extern char g_ota_version[OTA_VERSION_MAX_LEN];

cmd_type_t parse_cmd_name(const char *name);

#endif // RTOS_RESOURCES_H
