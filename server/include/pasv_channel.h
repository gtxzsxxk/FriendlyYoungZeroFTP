//
// Created by hanyuan on 2024/10/10.
//

#ifndef SERVER_PASV_CHANNEL_H
#define SERVER_PASV_CHANNEL_H

#include <string.h>
#include "protocol.h"

#define MAX_ATTEMPTS     100
#define PORT_RANGE_START 20000
#define PORT_RANGE_END   60000

int pasv_client_new(int *port);

int pasv_send_data(int port, const char *data, size_t len,
                   struct client_data *ctrl_client,
                   const char *end_msg);

void pasv_start(void);

#endif //SERVER_PASV_CHANNEL_H
