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
#include "esp_netif_ip_addr.h"
#include "driver/gpio.h"
#include "lwip/netdb.h"
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

static uint32_t s_reconnect_delay_ms = MQTT_RECONNECT_DELAY_MS;

static void reset_reconnect_backoff(void)
{
    s_reconnect_delay_ms = MQTT_RECONNECT_DELAY_MS;
}

static uint32_t next_reconnect_backoff_ms(void)
{
    uint32_t delay = s_reconnect_delay_ms;
    if (s_reconnect_delay_ms < 60000U) {
        s_reconnect_delay_ms = s_reconnect_delay_ms * 2U;
        if (s_reconnect_delay_ms > 60000U) {
            s_reconnect_delay_ms = 60000U;
        }
    }
    return delay;
}

static bool is_wifi_connected(void)
{
    if (g_system_event_group == NULL) {
        return false;
    }
    EventBits_t bits = xEventGroupGetBits(g_system_event_group);
    return (bits & EVT_WIFI_CONNECTED) != 0;
}

static void log_dns_servers(void)
{
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif == NULL) {
        ESP_LOGW(TAG, "Cannot query DNS servers: STA netif not found");
        return;
    }

    esp_netif_dns_info_t dns = {0};

    if (esp_netif_get_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns) == ESP_OK &&
        dns.ip.type == ESP_IPADDR_TYPE_V4) {
        ESP_LOGI(TAG, "DNS main: " IPSTR, IP2STR(&dns.ip.u_addr.ip4));
    }

    if (esp_netif_get_dns_info(netif, ESP_NETIF_DNS_BACKUP, &dns) == ESP_OK &&
        dns.ip.type == ESP_IPADDR_TYPE_V4) {
        ESP_LOGI(TAG, "DNS backup: " IPSTR, IP2STR(&dns.ip.u_addr.ip4));
    }

    if (esp_netif_get_dns_info(netif, ESP_NETIF_DNS_FALLBACK, &dns) == ESP_OK &&
        dns.ip.type == ESP_IPADDR_TYPE_V4) {
        ESP_LOGI(TAG, "DNS fallback: " IPSTR, IP2STR(&dns.ip.u_addr.ip4));
    }
}

static bool wait_for_dns_and_internet_ready(const char *host)
{
    const uint32_t probe_interval_ms = 1000U;
    const uint32_t max_wait_ms = 15000U;
    uint32_t elapsed_ms = 0U;

    while (elapsed_ms < max_wait_ms) {
        struct addrinfo hints = {0};
        struct addrinfo *res = NULL;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        int err = getaddrinfo(host, NULL, &hints, &res);
        if (err == 0 && res != NULL) {
            freeaddrinfo(res);
            return true;
        }

        if (res != NULL) {
            freeaddrinfo(res);
        }

        ESP_LOGW(TAG, "DNS check failed for %s: getaddrinfo=%d", host, err);
        vTaskDelay(pdMS_TO_TICKS(probe_interval_ms));
        elapsed_ms += probe_interval_ms;
    }

    return false;
}

static bool wait_for_ip_stable(uint32_t max_wait_ms)
{
    const uint32_t probe_ms = 500U;
    uint32_t elapsed_ms = 0U;

    while (elapsed_ms < max_wait_ms) {
        if (is_wifi_connected()) {
            esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
            if (netif != NULL) {
                esp_netif_ip_info_t ip_info;
                if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK && ip_info.ip.addr != 0) {
                    // Give routing stack a short settle window after got-ip.
                    vTaskDelay(pdMS_TO_TICKS(1500U));
                    return true;
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(probe_ms));
        elapsed_ms += probe_ms;
    }

    return false;
}


static int32_t tls_recv(NetworkContext_t *ctx, void *buf, size_t len){
    ssize_t n = esp_tls_conn_read(ctx->tls, buf, len);
    if (n > 0)  return (int32_t)n;
    if (n == 0) return -1;
    if (n == ESP_TLS_ERR_SSL_WANT_READ) return 0;
    if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;

    // Downgrade expected socket errors when Wi-Fi link is down.
    if (!is_wifi_connected() &&
        (errno == ENETUNREACH || errno == ENETDOWN || errno == ENOTCONN || errno == ECONNRESET)) {
        ESP_LOGW(TAG, "TLS recv interrupted by WiFi disconnect (errno=%d)", errno);
        return -1;
    }

    ESP_LOGE(TAG, "TLS recv error %d (errno=%d)", (int)n, errno);
    return -1;
}

static int32_t tls_send(NetworkContext_t *ctx, const void *buf, size_t len){
    ssize_t n = esp_tls_conn_write(ctx->tls, buf, len);
    if (n > 0)  return (int32_t)n;
    if (n == 0) return -1;
    if (n == ESP_TLS_ERR_SSL_WANT_WRITE) return 0;
    if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;

    // Downgrade expected socket errors when Wi-Fi link is down.
    if (!is_wifi_connected() &&
        (errno == ENETUNREACH || errno == ENETDOWN || errno == ENOTCONN || errno == ECONNRESET)) {
        ESP_LOGW(TAG, "TLS send interrupted by WiFi disconnect (errno=%d)", errno);
        return -1;
    }

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
        .timeout_ms       = 15000,
        .non_block        = false,
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

    // Keep recv timeout short so MQTT_Connect can retry within its deadline.
    int sock = -1;
    if (esp_tls_get_conn_sockfd(tls, &sock) == ESP_OK && sock >= 0) {
        struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
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

    // Push raw received message into subscribe queue for other tasks to consume.
    if (g_mqtt_subscribe_queue != NULL) {
        mqtt_subscribe_msg_t sub_msg;
        memset(&sub_msg, 0, sizeof(sub_msg));

        size_t top_copy = pub->topicNameLength < sizeof(sub_msg.topic) - 1
                          ? pub->topicNameLength : sizeof(sub_msg.topic) - 1;
        memcpy(sub_msg.topic, pub->pTopicName, top_copy);
        sub_msg.topic[top_copy] = '\0';

        size_t pay_copy = pub->payloadLength < sizeof(sub_msg.payload) - 1
                          ? pub->payloadLength : sizeof(sub_msg.payload) - 1;
        memcpy(sub_msg.payload, pub->pPayload, pay_copy);
        sub_msg.payload[pay_copy] = '\0';
        sub_msg.payload_len = pay_copy;

        if (xQueueSend(g_mqtt_subscribe_queue, &sub_msg, 0) != pdTRUE) {
            ESP_LOGW(TAG, "Subscribe queue full - message dropped");
        }
    }

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

    subs[0].qos               = MQTTQoS0;
    subs[0].pTopicFilter      = TOPIC_CMD;
    subs[0].topicFilterLength = (uint16_t)strlen(TOPIC_CMD);

    subs[1].qos               = MQTTQoS0;
    subs[1].pTopicFilter      = TOPIC_OTA_CMD;
    subs[1].topicFilterLength = (uint16_t)strlen(TOPIC_OTA_CMD);

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
    reset_reconnect_backoff();

    while (1) {
        if (!wait_for_ip_stable(10000U)) {
            uint32_t delay = next_reconnect_backoff_ms();
            ESP_LOGW(TAG, "IP not stable yet, retry in %lu ms", (unsigned long)delay);
            vTaskDelay(pdMS_TO_TICKS(delay));
            continue;
        }

        // Check DNS/internet path before opening TLS.
        log_dns_servers();
        if (!wait_for_dns_and_internet_ready(AWS_ENDPOINT)) {
            uint32_t delay = next_reconnect_backoff_ms();
            ESP_LOGE(TAG, "DNS/internet not ready, retry in %lu ms", (unsigned long)delay);
            vTaskDelay(pdMS_TO_TICKS(delay));
            continue;
        }

        //TLS connect 
        s_net_ctx.tls = tlsconnect(AWS_ENDPOINT, AWS_PORT);
        if (s_net_ctx.tls == NULL) {
            uint32_t delay = next_reconnect_backoff_ms();
            ESP_LOGE(TAG, "TLS connect failed, retry in %lu ms", (unsigned long)delay);
            vTaskDelay(pdMS_TO_TICKS(delay));
            continue;
        }

        //MQTT connect 
        MQTTStatus_t status = mqtt_connect(&mqtt_ctx, &s_net_ctx,
                                            s_net_buf, sizeof(s_net_buf));
        if (status != MQTTSuccess) {
            uint32_t delay = next_reconnect_backoff_ms();
            ESP_LOGE(TAG, "MQTT_Connect failed: %d, retry in %lu ms",
                     status, (unsigned long)delay);
            tlsdisconnect(s_net_ctx.tls);
            vTaskDelay(pdMS_TO_TICKS(delay));
            continue;
        }

        ESP_LOGI(TAG, "MQTT connected");
        reset_reconnect_backoff();

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
                if (is_wifi_connected()) {
                    ESP_LOGE(TAG, "MQTT_ProcessLoop error: %d", status);
                } else {
                    ESP_LOGW(TAG, "MQTT loop interrupted by WiFi disconnect");
                }
                break;
            }

            vTaskDelay(pdMS_TO_TICKS(MQTT_PROCESS_LOOP_MS));
        }

        //Cleanup on disconnect
        xEventGroupClearBits(g_system_event_group, EVT_MQTT_CONNECTED);
        tlsdisconnect(s_net_ctx.tls);

        if (is_wifi_connected()) {
            uint32_t delay = next_reconnect_backoff_ms();
            ESP_LOGW(TAG, "MQTT disconnected, reconnecting in %lu ms", (unsigned long)delay);
            vTaskDelay(pdMS_TO_TICKS(delay));
        } else {
            ESP_LOGW(TAG, "MQTT disconnected, waiting for WiFi");
        }

        // Re-wait for WiFi
        xEventGroupWaitBits(g_system_event_group, EVT_WIFI_CONNECTED, pdFALSE, pdTRUE, portMAX_DELAY);
    }
}