#include "socket_manager.h"
#include "esp_log.h"

static const char* TAG = "Socket";

int socket_create() {
    // create socket object
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock >= 0) {
        ESP_LOGI(TAG, "Socket created successfully, ID=%d", sock);
    }
    else {
        ESP_LOGE(TAG, "Socket creation failed! errno=%d (%s)", errno, strerror(errno));
    }
    return sock;    
}

void socket_connect(int sock) {
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr("146.190.62.39");
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(80);

    printf("Connecting...\n");
    int err = connect(sock, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    if(err == 0) {
        ESP_LOGI(TAG, "Socket connect succesfully to %s:%d", inet_ntoa(dest_addr.sin_addr), ntohs(dest_addr.sin_port));
    } 
    else {
        ESP_LOGE(TAG, "Socket connect failed, errno=%d (%s)", errno, strerror(errno));
    }
}

/** 
 * @brief A wraper to send data through a network socket
 * @param[in]   socket    The socket file descriptor.
 * @param[out]  pBuffer   Pointer to the buffer containing data to be sent. 
 * @param[in]   len       The size of data in pBuffer
 * 
 * @retval >=0          Size of data sent
 * @retval -1           Error
 */
int32_t socket_send(int socket, const void* pBuffer, size_t len) {
    return send(socket, pBuffer, len, 0);
}

/** 
 * @brief A wraper to receive data through a network socket
 * @param[in]   socket    The socket file descriptor.
 * @param[out]  pBuffer   Pointer to the buffer containing data to be received. 
 * @param[in]   len       The size of data in pBuffer
 * 
 * @retval >=0          Size of data received
 * @retval -1           Error
 */
int32_t socket_recv(int socket, void* pBuffer, size_t len) {
    return recv(socket, pBuffer, len, 0);
}

int socket_create_connect() {
    int sock = socket_create();

    socket_connect(sock);

    return sock;
}