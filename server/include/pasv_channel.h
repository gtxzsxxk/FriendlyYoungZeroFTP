//
// Created by hanyuan on 2024/10/10.
//

#ifndef SERVER_PASV_CHANNEL_H
#define SERVER_PASV_CHANNEL_H

#include <string.h>
#include <unistd.h>
#include "protocol.h"

#define MAX_ATTEMPTS     100
#define PORT_RANGE_START 20000
#define PORT_RANGE_END   60000

#define MAX_SEND_CHUNK 65536

int pasv_client_new(int *port);

int pasv_send_set_rest(int port, off_t offset);

int pasv_send_data(int port, const char *data, size_t len,
                   struct client_data *ctrl_client,
                   const char *end_msg);

int pasv_sendfile(int port, const char *path,
                  struct client_data *ctrl_client,
                  const char *end_msg);

int pasv_recvfile(int port, const char *path,
                  struct client_data *ctrl_client,
                  const char *end_msg);

int pasv_close_connection(int port);

void pasv_start(void);

#endif //SERVER_PASV_CHANNEL_H
