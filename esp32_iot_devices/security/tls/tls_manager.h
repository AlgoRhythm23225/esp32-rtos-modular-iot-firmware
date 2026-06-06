#ifndef TLS_MANAGER_H
#define TLS_MANAGER_H

#include "esp_tls.h"
#include "esp_crt_bundle.h"

/** 
 * @brief Send data using TLS
 */
int32_t tls_send(esp_tls_t *tls_handle, const void *pBuffer, size_t len);

/** 
 * @brief Read data using TLS
 */
int32_t tls_recv(esp_tls_t *tls_handle, void *pBuffer, size_t len);

#endif