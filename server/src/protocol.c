//
// Created by hanyuan on 2024/10/9.
//

#include "../include/protocol.h"
#include <string.h>
#include "logger.h"

const char *BANNER_STRING = "Welcome to Friendly Young Zero FTP Server!";

static struct client_data clients[MAX_CLIENTS] = {0};

static int protocol_client_index_by_fd(int fd) {
    /* TODO: 性能关键路径，用 hash */
    int latest_free = -1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].sock_fd == fd) {
            return i;
        } else if (clients[i].sock_fd == 0 && latest_free == -1) {
            latest_free = i;
        }
    }

    return latest_free;
}

struct client_data *protocol_client_init(int fd, int nfds) {
    /* 分配一个新的 client_data */
    int index = protocol_client_index_by_fd(fd);
    clients[index].sock_fd = fd;
    clients[index].state = NEED_SEND;
    clients[index].nfds = nfds;
    strcpy(&(clients[index].cmd_send[0]), BANNER_STRING);

    return &clients[index];
}

void protocol_client_free(int fd) {
    int index = protocol_client_index_by_fd(fd);
    clients[index].sock_fd = 0;
}

struct client_data *protocol_client_by_fd(int fd) {
    /* 分配一个新的 client_data */
    int index = protocol_client_index_by_fd(fd);
    if (!clients[index].sock_fd) {
        return NULL;
    }

    return &clients[index];
}

void protocol_on_recv(int fd) {
    struct client_data *client = protocol_client_by_fd(fd);
    logger_info("receive data: %s", client->cmd_request);
}
