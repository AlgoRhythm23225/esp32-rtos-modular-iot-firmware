#include "wifi_task.h"
#include "wifi_config.h"
#include "rtos_config.h"

#include <stdbool.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

static const char *TAG = "WIFI_TASK";

static int s_retry_num = 0;
static uint32_t s_retry_delay_ms = WIFI_RETRY_BASE_DELAY_MS;
static esp_timer_handle_t s_reconnect_timer = NULL;
static bool s_reconnect_scheduled = false;

static void reset_retry_backoff(void)
{
    s_retry_num = 0;
    s_retry_delay_ms = WIFI_RETRY_BASE_DELAY_MS;
}

static uint32_t get_next_retry_delay_ms(void)
{
    uint32_t delay = s_retry_delay_ms;
    if (s_retry_delay_ms < WIFI_RETRY_MAX_DELAY_MS) {
        s_retry_delay_ms = s_retry_delay_ms * 2U;
        if (s_retry_delay_ms > WIFI_RETRY_MAX_DELAY_MS) {
            s_retry_delay_ms = WIFI_RETRY_MAX_DELAY_MS;
        }
    }
    s_retry_num++;
    return delay;
}

static void wifi_reconnect_timer_cb(void *arg)
{
    (void)arg;
    s_reconnect_scheduled = false;
    ESP_LOGI(TAG, "Starting WiFi connect");
    esp_err_t ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "esp_wifi_connect failed: %s", esp_err_to_name(ret));
    }
}

static esp_err_t ensure_reconnect_timer(void)
{
    if (s_reconnect_timer != NULL) {
        return ESP_OK;
    }

    const esp_timer_create_args_t timer_args = {
        .callback = &wifi_reconnect_timer_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "wifi_reconn",
        .skip_unhandled_events = true,
    };

    return esp_timer_create(&timer_args, &s_reconnect_timer);
}

static void schedule_wifi_reconnect(void)
{
    esp_err_t ret = ensure_reconnect_timer();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Reconnect timer init failed: %s", esp_err_to_name(ret));
        return;
    }

    if (s_reconnect_scheduled) {
        return;
    }

    uint32_t delay_ms = get_next_retry_delay_ms();
    ESP_LOGW(TAG, "WiFi retry %d in %lu ms", s_retry_num, (unsigned long)delay_ms);

    ret = esp_timer_start_once(s_reconnect_timer, (uint64_t)delay_ms * 1000ULL);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Reconnect timer start failed: %s", esp_err_to_name(ret));
        return;
    }

    s_reconnect_scheduled = true;
}

static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    (void)arg;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        reset_retry_backoff();
        ESP_LOGI(TAG, "Starting WiFi connect");
        esp_wifi_connect();
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(g_system_event_group, EVT_WIFI_CONNECTED);

        // Use exponential backoff with no hard retry limit.
        schedule_wifi_reconnect();
        return;
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;

        if (s_reconnect_timer != NULL && s_reconnect_scheduled) {
            esp_timer_stop(s_reconnect_timer);
            s_reconnect_scheduled = false;
        }

        reset_retry_backoff();
        xEventGroupSetBits(g_system_event_group, EVT_WIFI_CONNECTED);
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

static esp_err_t wifi_init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

esp_err_t wifi_start(void)
{
    if (strcmp(WIFI_SSID, "YOUR_WIFI_SSID") == 0) {
        ESP_LOGE(TAG, "Please update wifi_config.h");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = wifi_init_nvs();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Netif init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Event loop init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_event_handler_instance_t wifi_instance_any_id;
    esp_event_handler_instance_t ip_instance_got_ip;

    ret = esp_event_handler_instance_register(WIFI_EVENT,
                                              ESP_EVENT_ANY_ID,
                                              &wifi_event_handler,
                                              NULL,
                                              &wifi_instance_any_id);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi handler register failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_event_handler_instance_register(IP_EVENT,
                                              IP_EVENT_STA_GOT_IP,
                                              &wifi_event_handler,
                                              NULL,
                                              &ip_instance_got_ip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "IP handler register failed: %s", esp_err_to_name(ret));
        return ret;
    }

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, WIFI_SSID, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, WIFI_PASSWORD, sizeof(wifi_config.sta.password) - 1);

    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Set WiFi mode failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Set WiFi config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi start failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Disable Wi-Fi power save to improve link stability.
    ret = esp_wifi_set_ps(WIFI_PS_NONE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Set WiFi power save failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "WiFi start requested for SSID: %s", WIFI_SSID);
    return ESP_OK;
}