#ifndef TLS_MANAGER_H
#define TLS_MANAGER_H

#include "esp_tls.h"

typedef int tls_err_t;

/** 
 * @brief Send data using TLS
 */
int32_t tls_send(esp_tls_t *tls_handle, const void *pBuffer, size_t len);

/** 
 * @brief Read data using TLS
 */
int32_t tls_recv(esp_tls_t *tls_handle, void *pBuffer, size_t len);

/**
 * @brief Init TLS Handle and connect to host
 * @param tls_Handle
 * @param[in] host to connect
 * @param[in] port of the host
 */
tls_err_t tls_init_connect(esp_tls_t **tls_handle, const char* host, const char *port);

#endif