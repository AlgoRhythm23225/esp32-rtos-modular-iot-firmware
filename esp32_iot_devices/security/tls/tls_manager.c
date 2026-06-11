#include <stdint.h>
#include "tls_manager.h"
#include "esp_log.h"
#include "esp_crt_bundle.h"


static const char* TAG = "TLS";

int32_t tls_send(esp_tls_t *tls_handle, const void *pBuffer, size_t len) {
    if (tls_handle == NULL) {
    return -1;
    }
    ssize_t ret = esp_tls_conn_write(tls_handle, pBuffer, len);
    if (ret < 0) {
        ESP_LOGE(TAG, "TSL write failed, error code: %d", (int)ret);
    } 

    return (int32_t)ret;    
}

int32_t tls_recv(esp_tls_t *tls_handle, void *pBuffer, size_t len) {
    if (tls_handle == NULL) {
        return -1;
    }
    ssize_t ret = esp_tls_conn_read(tls_handle, pBuffer, len);
    if (ret < 0) {
        if (ret == ESP_TLS_ERR_SSL_WANT_READ || ret == ESP_TLS_ERR_SSL_WANT_WRITE) {
            return 0;
        }
        ESP_LOGE(TAG, "TLS read failed, error code: %d", (int)ret);
    }
    return (int32_t)ret;
}  

tls_err_t tls_init_connect(esp_tls_t **tls_handle, const char* host, const char *port) {
    esp_tls_cfg_t cfg = {
        .use_global_ca_store = false,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    *tls_handle = esp_tls_init();
    if (*tls_handle == NULL) {
        ESP_LOGE(TAG, "Failed to initialize ESP_TLS structure");
        return -1;
    }

    ESP_LOGI(TAG, "Connecting to %s:%s via TLS...", host, port);
    tls_err_t ret = esp_tls_conn_new_sync(host, strlen(host), atoi(port), &cfg, *tls_handle);

    return ret;
}
