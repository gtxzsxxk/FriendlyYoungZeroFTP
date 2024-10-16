//
// Created by hanyuan on 2024/10/10.
//

#ifndef SERVER_PORT_CHANNEL_H
#define SERVER_PORT_CHANNEL_H

#include <string.h>
#include "protocol.h"
#include "pasv_channel.h"

int port_client_new(struct sockaddr_in target_addr);

int port_send_data(struct client_data *ctrl_client,
                   const char *data,
                   size_t len,
                   const char *end_msg);

int port_sendfile(struct client_data *ctrl_client,
                  const char *path,
                  const char *end_msg);

int port_recvfile(struct client_data *ctrl_client,
                  const char *path,
                  const char *end_msg);

void port_start(void);

#endif //SERVER_PORT_CHANNEL_H
