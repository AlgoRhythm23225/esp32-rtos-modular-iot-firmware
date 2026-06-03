#include "http_client.h"
#include "esp_log.h"

int32_t myPlatformTransportSend(NetworkContext_t *pNetworkContext,
                                            const void *pBuffer,
                                            size_t bytesToSend) 
{
    return socket_send(pNetworkContext->socket, pBuffer, bytesToSend);
}
    
int32_t myPlatformTransportReceive(NetworkContext_t *pNetworkContext,
                                           void *pBuffer,
                                           size_t bytesToRecv) 
{
    return socket_recv(pNetworkContext->socket, pBuffer, bytesToRecv);
}

// Declare an HTTPRequestHeaders_t and HTTPRequestInfo_t.
HTTPRequestHeaders_t pRequestHeaders = {0};
uint8_t requestHeaderBuffer[256] = { 0 };
void http_req_headers_init() {
    HTTPStatus_t httpLibraryStatus = HTTPSuccess;
    HTTPRequestInfo_t requestInfo = {0};
    // A buffer that will fit the Request-Line, the User-Agent header line, and
    // the Host header line.

    // Set a buffer to serialize request headers to.
    pRequestHeaders.pBuffer = requestHeaderBuffer;
    pRequestHeaders.bufferLen = sizeof(requestHeaderBuffer);

    // Set the Method, Path, and Host in the HTTPRequestInfo_t.
    requestInfo.pMethod = HTTP_METHOD_GET;
    requestInfo.methodLen = sizeof( HTTP_METHOD_GET ) - 1U;
    requestInfo.pPath = "/";
    requestInfo.pathLen = sizeof( "/" ) - 1U;
    requestInfo.pHost = "httpforever.com";
    requestInfo.hostLen = sizeof( "httpforever.com" ) - 1U;
    requestInfo.reqFlags |= HTTP_REQUEST_KEEP_ALIVE_FLAG;

    httpLibraryStatus = HTTPClient_InitializeRequestHeaders( &pRequestHeaders, &requestInfo );
    if(httpLibraryStatus == HTTPSuccess) {
        ESP_LOGI("HTTP_Client", "Init request header success!");
    }
    else {
        ESP_LOGE("HTTP_Client", "Error=%d (%s)", httpLibraryStatus, HTTPClient_strerror(httpLibraryStatus));
    }

}

void http_send(int sock) {
    HTTPStatus_t httpLibraryStatus = HTTPSuccess;
    TransportInterface_t pTransport = { 0 };
    HTTPResponse_t pResponse = { 0 };
    NetworkContext_t ctx;
    ctx.socket = sock;
    
    // Set the transport interface with platform specific functions that are
    // assumed to be implemented elsewhere.
    pTransport.recv = myPlatformTransportReceive;
    pTransport.send = myPlatformTransportSend;
    pTransport.pNetworkContext = &ctx;

    // Set the buffer to receive the HTTP response message into. The buffer is
    // dynamically allocated for demonstration purposes only.
    pResponse.pBuffer = (uint8_t*)malloc( 8124 );
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
            ESP_LOGI("HTTP_Client", "Send request headers success!");
        }
        else
        {
            ESP_LOGE("HTTP_Client", "Didn't send, status code=%d", pResponse.statusCode);
        }
    }
    else {
        ESP_LOGE("HTTP_Client", "Send request headers failed, ID=%d (%s)", httpLibraryStatus, HTTPClient_strerror(httpLibraryStatus));
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
    close(sock);
}

void http_test_task() {
    http_req_headers_init();

    int sock = socket_create_connect();
    http_send(sock);
}
