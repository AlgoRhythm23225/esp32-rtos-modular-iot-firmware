#include "http_client.h"
#include "esp_log.h"
#include "core_http_client.h"
#include "socket_manager.h"
#include "tls_manager.h"

static const char* TAG_TCP = "HTTP_TCP_CLIENT";
static const char* TAG_TLS = "HTTP_TLS_CLIENT";

// Choose transport type TCP or TLS
static transport_type_t g_transport_type = TRANSPORT_TLS;

// Alternative for ESP_LOGI
#define HTTP_LOGI(format, ...) do {                                                     \
    if (g_transport_type == TRANSPORT_TCP) {                                            \
        ESP_LOGI(TAG_TCP, format, ##__VA_ARGS__);                                       \
    } else {                                                                            \
        ESP_LOGI(TAG_TLS, format, ##__VA_ARGS__);                                       \
    }                                                                                   \
} while(0)

// Alternative for ESP_LOGE
#define HTTP_LOGE(format, ...) do {                                                     \
    if (g_transport_type == TRANSPORT_TCP) {                                            \
        ESP_LOGE(TAG_TCP, format, ##__VA_ARGS__);                                       \
    } else {                                                                            \
        ESP_LOGE(TAG_TLS, format, ##__VA_ARGS__);                                       \
    }                                                                                   \
} while(0)

/**
 * @brief Interface for send
 */
int32_t myPlatformTransportSend(NetworkContext_t *pNetworkContext,
                                            const void *pBuffer,
                                            size_t bytesToSend) 
{
    if (pNetworkContext == NULL) {
        return -1;
    }
    // TCP
    if (g_transport_type == TRANSPORT_TCP) {
        return socket_send(pNetworkContext->socket, pBuffer, bytesToSend);
    }
    // TLS 
    else {
        return tls_send(pNetworkContext->tls_handle, pBuffer, bytesToSend);
    }
}
    
/**
 * @brief Interface for read
 */
int32_t myPlatformTransportReceive(NetworkContext_t *pNetworkContext,
                                           void *pBuffer,
                                           size_t bytesToRecv) 
{
    if (pNetworkContext == NULL) {
    return -1;
    }
    // TCP
    if (g_transport_type == TRANSPORT_TCP) {
        return socket_recv(pNetworkContext->socket, pBuffer, bytesToRecv);
    }
    //TLS
    else {
        return tls_recv(pNetworkContext->tls_handle, pBuffer, bytesToRecv);
    }
}

// Declare an HTTPRequestHeaders_t and HTTPRequestInfo_t.
HTTPRequestHeaders_t pRequestHeaders = {0};
uint8_t requestHeaderBuffer[256] = { 0 };
void http_req_headers_init(const char *host) {
    HTTPStatus_t httpLibraryStatus = HTTPSuccess;
    HTTPRequestInfo_t requestInfo = {0};
    // A buffer that will fit the Request-Line, the User-Agent header line, and
    // the Host header line.

    // Set a buffer to serialize request headers to.
    pRequestHeaders.pBuffer = requestHeaderBuffer;
    pRequestHeaders.bufferLen = sizeof(requestHeaderBuffer);

    // Set the Method, Path, and Host in the HTTPRequestInfo_t.
    requestInfo.pMethod = HTTP_METHOD_GET;
    requestInfo.methodLen = sizeof(HTTP_METHOD_GET) - 1U;
    requestInfo.pPath = "/";
    requestInfo.pathLen = sizeof("/") - 1U;
    requestInfo.pHost = host;
    requestInfo.hostLen = strlen(host);
    requestInfo.reqFlags |= HTTP_REQUEST_KEEP_ALIVE_FLAG;

    httpLibraryStatus = HTTPClient_InitializeRequestHeaders( &pRequestHeaders, &requestInfo );
    if(httpLibraryStatus == HTTPSuccess) {
        HTTP_LOGI("Init request header success!");
    }
    else {
        HTTP_LOGE("Error=%d (%s)", httpLibraryStatus, HTTPClient_strerror(httpLibraryStatus));
    }

}

void http_send(NetworkContext_t *pNetworkContext) {
    HTTPStatus_t httpLibraryStatus = HTTPSuccess;
    TransportInterface_t pTransport = { 0 };
    HTTPResponse_t pResponse = { 0 };
    
    // Set the transport interface with platform specific functions that are
    // assumed to be implemented elsewhere.
    pTransport.recv = myPlatformTransportReceive;
    pTransport.send = myPlatformTransportSend;   
    pTransport.pNetworkContext = pNetworkContext;


    // Set the buffer to receive the HTTP response message into. The buffer is
    // dynamically allocated for demonstration purposes only.
    pResponse.pBuffer = (uint8_t*)malloc( 8124 );
    if (pResponse.pBuffer == NULL) {
        HTTP_LOGE("Failed to allocate memory for response buffer");
        return;
    }
    pResponse.bufferLen = 8124;

    httpLibraryStatus = HTTPClient_Send(&pTransport,
                                        &pRequestHeaders,
                                        NULL,
                                        0,
                                        &pResponse,
                                        0);

    if( httpLibraryStatus == HTTPSuccess )
    {
        if( pResponse.statusCode == 200 )
        {
            HTTP_LOGI("Send request headers success!");
        }
        else
        {
            HTTP_LOGE("Didn't send, status code=%d", pResponse.statusCode);
        }
    }
    else {
        HTTP_LOGE("Send request headers failed, ID=%d (%s)", httpLibraryStatus, HTTPClient_strerror(httpLibraryStatus));
    }

    const char *pDateLoc = NULL;
    size_t dateLen = 0;
    httpLibraryStatus = HTTPClient_ReadHeader(&pResponse,
                                                "Date",
                                                sizeof("Date") - 1,
                                                &pDateLoc,
                                                &dateLen);
    
    printf("\033[36m===REQUEST HEADERS===\033[0m\n");
    printf("%.*s\n", (int)pResponse.headersLen, pResponse.pHeaders);
    printf("\033[36m===BODY===\033[0m\n");
    printf("%.*s\n", (int)pResponse.bodyLen, pResponse.pBody);
    
    free(pResponse.pBuffer);

    // TCP
    if (g_transport_type == TRANSPORT_TCP) {
        close(pNetworkContext->socket);
    }
}

void http_test_task() {
    const char *host = "tls-v1-2.badssl.com";
    const char *port = "1012";

    http_req_headers_init(host);
    NetworkContext_t ctx = {
        .socket = 0,
        .tls_handle = NULL,
    };

    // TCP
    if (g_transport_type == TRANSPORT_TCP) {
        ctx.socket = socket_create_connect();
        http_send(&ctx);
    }
    // TLS
    else {
        esp_tls_cfg_t cfg = {
            .use_global_ca_store = false,
            .crt_bundle_attach = esp_crt_bundle_attach,
        };

        ctx.tls_handle = esp_tls_init();
        if (ctx.tls_handle == NULL) {
            ESP_LOGE(TAG_TLS, "Failed to initialize ESP_TLS structure");
            return;
        }

        HTTP_LOGI("Connecting to %s:%s via TLS...", host, port);
        int ret = esp_tls_conn_new_sync(host, strlen(host), atoi(port), &cfg, ctx.tls_handle);

        if (ret > 0) {
            ESP_LOGI(TAG_TLS, "TLS Connected Successfully!");
            
            http_send(&ctx);

            esp_tls_conn_destroy(ctx.tls_handle);
            ESP_LOGI(TAG_TLS, "TLS Connection closed!");
        }
        else {
            ESP_LOGI(TAG_TLS, "Failed to establish TLS connection!");
            esp_tls_conn_destroy(ctx.tls_handle);
        }        
    }
}
