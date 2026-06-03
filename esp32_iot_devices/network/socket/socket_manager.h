#ifndef SOCKET_MANAGER_H
#define SOCKET_MANAGER_H

#include "lwip/sockets.h"

int socket_create();
void socket_connect(int sock);
int32_t socket_send(int socket, const void* pBuffer, size_t len);
int32_t socket_recv(int socket, void* pBuffer, size_t len);

int socket_create_connect();

#endif