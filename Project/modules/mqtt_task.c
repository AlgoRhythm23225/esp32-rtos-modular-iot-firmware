#include "mqtt_task.h"
#include "rtos_config.h"
#include "core_mqtt.h"
#include "string.h"
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_tls.h"
#include "driver/gpio.h"
#include <stdlib.h>

#define CORE_MQTT_CONFIG_H_

#define LogError( message )
#define LogWarn( message )
#define LogInfo( message )
#define LogDebug( message )


//Certificate for secure connection
extern const uint8_t aws_root_ca_start[] asm("_binary_AmazonRootCA1_pem_start");
extern const uint8_t aws_root_ca_end[]   asm("_binary_AmazonRootCA1_pem_end");
extern const uint8_t device_crt_start[]  asm("_binary_device_crt_start");
extern const uint8_t device_crt_end[]    asm("_binary_device_crt_end");
extern const uint8_t device_key_start[]  asm("_binary_device_key_start");
extern const uint8_t device_key_end[]    asm("_binary_device_key_end");

static const char *TAG = "MQTT_TASK";

static bool s_ota_pending        = false;
char g_ota_url[OTA_URL_MAX_LEN]         = {0};
char g_ota_version[OTA_VERSION_MAX_LEN] = {0};

cmd_type_t parse_cmd_name(const char *name)
{
    if (strcmp(name, "gpio_set") == 0)   return CMD_GPIO_SET;
    if (strcmp(name, "gpio_reset") == 0) return CMD_GPIO_RESET;
    if (strcmp(name, "reboot") == 0)     return CMD_REBOOT;
    return CMD_UNKNOWN;
}

struct NetworkContext{
    esp_tls_t* tls;
};
typedef struct NetworkContext NetworkContext_t;

static MQTTContext_t mqtt_ctx;


static int32_t tls_recv(NetworkContext_t *ctx, void *buf, size_t len){
    ssize_t n = esp_tls_conn_read(ctx->tls, buf, len);
    if (n > 0)  return (int32_t)n;
    if (n == 0) return -1;
    if (n == ESP_TLS_ERR_SSL_WANT_READ) return 0;
    if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
    ESP_LOGE(TAG, "TLS recv error %d (errno=%d)", (int)n, errno);
    return -1;
}

static int32_t tls_send(NetworkContext_t *ctx, const void *buf, size_t len){
    ssize_t n = esp_tls_conn_write(ctx->tls, buf, len);
    if (n > 0)  return (int32_t)n;
    if (n == 0) return -1;
    if (n == ESP_TLS_ERR_SSL_WANT_WRITE) return 0;
    if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
    ESP_LOGE(TAG, "TLS send error %d (errno=%d)", (int)n, errno);
    return -1;
}

static uint32_t get_time_ms(){
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

static esp_tls_t * tlsconnect(const char *host, const char *port){
    esp_tls_cfg_t cfg = {
        .cacert_buf       = aws_root_ca_start,
        .cacert_bytes     = (unsigned int)(aws_root_ca_end - aws_root_ca_start),
        .clientcert_buf   = device_crt_start,
        .clientcert_bytes = (unsigned int)(device_crt_end - device_crt_start),
        .clientkey_buf    = device_key_start,
        .clientkey_bytes  = (unsigned int)(device_key_end - device_key_start),
    };
    esp_tls_t *tls = esp_tls_init();
    if (tls == NULL) {
        ESP_LOGE(TAG, "Failed to allocate TLS context");
        return NULL;
    }
    if (esp_tls_conn_new_sync(host, strlen(host), atoi(port), &cfg, tls) != 1) {
        ESP_LOGE(TAG, "Failed to connect to %s:%s", host, port);
        esp_tls_conn_destroy(tls);
        return NULL;
    }
    return tls;
}

static void tlsdisconnect(esp_tls_t *tls){
    esp_tls_conn_destroy(tls);  
}

static bool json_get_string(const char *json, size_t json_len,
                             const char *key, char *out, size_t out_len)
{
    char search[72];
    int klen = snprintf(search, sizeof(search), "\"%s\":", key);
    if (klen <= 0 || (size_t)klen >= sizeof(search)) return false;

    const char *p = NULL;
    for (size_t i = 0; i + (size_t)klen <= json_len; i++) {
        if (memcmp(json + i, search, (size_t)klen) == 0) {
            const char *q = json + i + (size_t)klen;
            size_t rem = json_len - (size_t)(q - json);
            while (rem > 0 && (*q == ' ' || *q == '\t')) { q++; rem--; }
            if (rem > 0 && *q == '"') { p = q + 1; break; }
        }
    }
    if (!p) return false;

    size_t rem = json_len - (size_t)(p - json);
    size_t n = 0;
    for (size_t i = 0; i < rem && n < out_len - 1; i++) {
        if (p[i] == '"') break;
        if (p[i] == '\\' && i + 1 < rem) { i++; out[n++] = p[i]; }
        else { out[n++] = p[i]; }
    }
    out[n] = '\0';
    return n > 0;
}

static void mqtt_event_cb(MQTTContext_t          *ctx,
                           MQTTPacketInfo_t       *packet,
                           MQTTDeserializedInfo_t *deser)
{
    (void)ctx;
    if ((packet->type & 0xF0U) != MQTT_PACKET_TYPE_PUBLISH) return;

    MQTTPublishInfo_t *pub = deser->pPublishInfo;
    ESP_LOGI(TAG, "RX topic=%.*s len=%u",
             (int)pub->topicNameLength, pub->pTopicName, (unsigned)pub->payloadLength);

    const char *payload  = (const char *)pub->pPayload;
    size_t      pay_len  = pub->payloadLength;
    uint16_t    top_len  = pub->topicNameLength;
    const char *topic    = pub->pTopicName;

    // Handle device command topic
    if (top_len == (uint16_t)strlen(TOPIC_CMD) &&
        memcmp(topic, TOPIC_CMD, top_len) == 0) {
        char cmd_name[32]  = {0};
        char gpio_str[8]   = {0};
        char dur_str[16]   = {0};

        if (!json_get_string(payload, pay_len, "cmd", cmd_name, sizeof(cmd_name))) {
            ESP_LOGW(TAG, "CMD message missing 'cmd' field");
            return;
        }

        control_cmd_t cmd = {0};
        cmd.cmd = parse_cmd_name(cmd_name);
        if (cmd.cmd == CMD_UNKNOWN) {
            ESP_LOGW(TAG, "Unknown command: %s", cmd_name);
            return;
        }

        if (json_get_string(payload, pay_len, "gpio", gpio_str, sizeof(gpio_str))) {
            cmd.gpio_pin = (uint8_t)atoi(gpio_str);
        }
        if (json_get_string(payload, pay_len, "duration", dur_str, sizeof(dur_str))) {
            cmd.duration_ms = (uint32_t)atol(dur_str);
        }

        if (xQueueSend(g_control_queue, &cmd, 0) != pdTRUE) {
            ESP_LOGW(TAG, "Control queue full - command dropped");
        } else {
            ESP_LOGI(TAG, "Command queued: %s gpio=%d", cmd_name, cmd.gpio_pin);
        }
        return;
    }

    // Handle OTA command topic
    if (top_len == (uint16_t)strlen(TOPIC_OTA_CMD) &&
        memcmp(topic, TOPIC_OTA_CMD, top_len) == 0) {
        if (s_ota_pending) {
            ESP_LOGW(TAG, "OTA already pending - ignoring duplicate");
            return;
        }
        char url_buf[1024] = {0};
        char ver_buf[48]   = {0};
        if (!json_get_string(payload, pay_len, "url", url_buf, sizeof(url_buf))
            || url_buf[0] == '\0') {
            ESP_LOGE(TAG, "OTA command missing 'url' field");
            return;
        }
        if (!json_get_string(payload, pay_len, "version", ver_buf, sizeof(ver_buf))) {
            strncpy(ver_buf, "unknown", sizeof(ver_buf) - 1);
        }
        strncpy(g_ota_url, url_buf, OTA_URL_MAX_LEN - 1);
        strncpy(g_ota_version, ver_buf, OTA_VERSION_MAX_LEN - 1);
        s_ota_pending = true;
        xEventGroupSetBits(g_system_event_group, EVT_OTA_IN_PROGRESS);
        ESP_LOGI(TAG, "OTA requested: version=%s", g_ota_version);
    }
}

static MQTTStatus_t mqtt_connect(MQTTContext_t *mqtt_ctx,
                                  NetworkContext_t *net_ctx,
                                  uint8_t *net_buf, size_t net_buf_len)
{
    TransportInterface_t transport = {
        .pNetworkContext = net_ctx,
        .recv            = tls_recv,
        .send            = tls_send,
        .writev          = NULL,
    };

    MQTTFixedBuffer_t fixed_buf = {
        .pBuffer = net_buf,
        .size    = net_buf_len,
    };

    MQTTStatus_t status = MQTT_Init(mqtt_ctx, &transport, get_time_ms,
                                     mqtt_event_cb, &fixed_buf);
    if (status != MQTTSuccess) {
        ESP_LOGE(TAG, "MQTT_Init failed: %d", status);
        return status;
    }

    MQTTConnectInfo_t conn_info = {
        .cleanSession        = true,
        .pClientIdentifier   = AWS_DEVICE_ID,
        .clientIdentifierLength = (uint16_t)strlen(AWS_DEVICE_ID),
        .keepAliveSeconds    = MQTT_KEEP_ALIVE_SEC,
    };

    bool session_present = false;
    status = MQTT_Connect(mqtt_ctx, &conn_info, NULL,
                          MQTT_CONNECT_TIMEOUT_MS, &session_present);
    return status;
}
static MQTTStatus_t mqtt_subscribe_all(MQTTContext_t *mqtt_ctx)
{
    MQTTSubscribeInfo_t subs[2];
    memset(subs, 0, sizeof(subs));

    subs[0].qos                 = MQTTQoS0;
    subs[0].pTopicFilter        = TOPIC_CMD;
    subs[0].topicFilterLength   = (uint16_t)strlen(TOPIC_CMD);

    subs[1].qos                 = MQTTQoS0;
    subs[1].pTopicFilter        = TOPIC_OTA_CMD;
    subs[1].topicFilterLength   = (uint16_t)strlen(TOPIC_OTA_CMD);

    return MQTT_Subscribe(mqtt_ctx, subs, 2, MQTT_GetPacketId(mqtt_ctx));
}

static void publish_pending_messages(MQTTContext_t *mqtt_ctx)
{
    mqtt_publish_msg_t msg;
    while (xQueueReceive(g_mqtt_publish_queue, &msg, 0) == pdTRUE) {
        MQTTPublishInfo_t pub = {
            .qos             = MQTTQoS0,
            .retain          = false,
            .pTopicName      = msg.topic,
            .topicNameLength = (uint16_t)strlen(msg.topic),
            .pPayload        = msg.payload,
            .payloadLength   = msg.payload_len,
        };
        MQTTStatus_t status = MQTT_Publish(mqtt_ctx, &pub, MQTT_GetPacketId(mqtt_ctx));
        if (status != MQTTSuccess) {
            ESP_LOGW(TAG, "MQTT_Publish failed: %d", status);
        }
    }
}
void mqtt_task(void *pvParameters)
{
    (void)pvParameters;

    static uint8_t s_net_buf[MQTT_NETWORK_BUF_SIZE];
    static NetworkContext_t s_net_ctx;
    memset(&s_net_ctx, 0, sizeof(s_net_ctx));

    // Wait for WiFi before attempting connection
    ESP_LOGI(TAG, "Waiting for WiFi...");
    xEventGroupWaitBits(g_system_event_group, EVT_WIFI_CONNECTED, pdFALSE, pdTRUE, portMAX_DELAY);
    ESP_LOGI(TAG, "WiFi ready, connecting to AWS IoT Core");

    while (1) {
        //TLS connect 
        s_net_ctx.tls = tlsconnect(AWS_ENDPOINT, AWS_PORT);
        if (s_net_ctx.tls == NULL) {
            ESP_LOGE(TAG, "TLS connect failed, retry in %d ms", MQTT_RECONNECT_DELAY_MS);
            vTaskDelay(pdMS_TO_TICKS(MQTT_RECONNECT_DELAY_MS));
            continue;
        }

        //MQTT connect 
        MQTTStatus_t status = mqtt_connect(&mqtt_ctx, &s_net_ctx,
                                            s_net_buf, sizeof(s_net_buf));
        if (status != MQTTSuccess) {
            ESP_LOGE(TAG, "MQTT_Connect failed: %d, retry in %d ms",
                     status, MQTT_RECONNECT_DELAY_MS);
            tlsdisconnect(s_net_ctx.tls);
            vTaskDelay(pdMS_TO_TICKS(MQTT_RECONNECT_DELAY_MS));
            continue;
        }

        ESP_LOGI(TAG, "MQTT connected");

        //Subscribe to command topics
        status = mqtt_subscribe_all(&mqtt_ctx);
        if (status != MQTTSuccess) {
            ESP_LOGW(TAG, "MQTT_Subscribe failed: %d", status);
        }

        // Signal other tasks that MQTT is ready
        xEventGroupSetBits(g_system_event_group, EVT_MQTT_CONNECTED);
        xSemaphoreGive(g_mqtt_ready_sem);


        while (1) {
            // Check if WiFi is still up
            EventBits_t bits = xEventGroupGetBits(g_system_event_group);
            if (!(bits & EVT_WIFI_CONNECTED)) {
                ESP_LOGW(TAG, "WiFi lost - disconnecting MQTT");
                break;
            }

            // Publish any queued messages
            publish_pending_messages(&mqtt_ctx);

            // Process incoming MQTT packets (runs mqtt_event_cb)
            status = MQTT_ProcessLoop(&mqtt_ctx);
            if (status != MQTTSuccess) {
                ESP_LOGE(TAG, "MQTT_ProcessLoop error: %d", status);
                break;
            }

            vTaskDelay(pdMS_TO_TICKS(MQTT_PROCESS_LOOP_MS));
        }

        //Cleanup on disconnect
        xEventGroupClearBits(g_system_event_group, EVT_MQTT_CONNECTED);
        tlsdisconnect(s_net_ctx.tls);

        ESP_LOGW(TAG, "MQTT disconnected, reconnecting in %d ms", MQTT_RECONNECT_DELAY_MS);
        vTaskDelay(pdMS_TO_TICKS(MQTT_RECONNECT_DELAY_MS));

        // Re-wait for WiFi
        xEventGroupWaitBits(g_system_event_group, EVT_WIFI_CONNECTED, pdFALSE, pdTRUE, portMAX_DELAY);
    }
}