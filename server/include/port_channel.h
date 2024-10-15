//
// Created by hanyuan on 2024/10/10.
//

#ifndef SERVER_PORT_CHANNEL_H
#define SERVER_PORT_CHANNEL_H

#include <string.h>
#include "protocol.h"
#include "pasv_channel.h"

int port_client_new(struct sockaddr_in target_addr);

int port_send_data(int port, const char *data, size_t len,
                   struct client_data *ctrl_client,
                   const char *end_msg);

int port_sendfile(int port, const char *path,
                  struct client_data *ctrl_client,
                  const char *end_msg);

int port_recvfile(int port, const char *path,
                  struct client_data *ctrl_client,
                  const char *end_msg);

void port_start(void);

#endif //SERVER_PORT_CHANNEL_H
