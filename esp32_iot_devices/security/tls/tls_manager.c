#include <stdint.h>
#include "tls_manager.h"
#include "esp_log.h"

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

