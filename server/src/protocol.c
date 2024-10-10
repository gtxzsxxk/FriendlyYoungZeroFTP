//
// Created by hanyuan on 2024/10/9.
//

#include "../include/protocol.h"
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "logger.h"
#include "ftp.h"

const char *BANNER_STRING = "Welcome to Friendly Young Zero FTP Server!";

char service_root[256];

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

void protocol_client_write_welcome_message(struct client_data *client, const char *data) {
    char *line;
    char *text = malloc(512);
    strcpy(text, data);
    int start = 0;
    for (line = strtok(text, "\r\n"); line; line = strtok(NULL, "\r\n")) {
        sprintf(client->cmd_send + start, "230-%s\r\n", line);
        start = strlen(client->cmd_send);
    }
    sprintf(client->cmd_send + start, "230 %s\r\n", "Guest login ok, access restrictions apply.");
    client->net_state = NEED_SEND;
    free(text);
}

struct client_data *protocol_client_init(int fd, int nfds) {
    /* 分配一个新的 client_data */
    int index = protocol_client_index_by_fd(fd);
    memset(&clients[index], 0, sizeof(struct client_data));
    clients[index].sock_fd = fd;
    clients[index].nfds = nfds;
    clients[index].cwd[0] = '/';

    protocol_client_write_response(&clients[index], 220, BANNER_STRING);

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
    logger_info("receive data: %s\r\n", client->cmd_request);
    char *command;
    command = strtok(client->cmd_request, " ");
    /* 命令全部转换为大写 */
    for (int i = 0; i < strlen(command); i++) {
        command[i] = toupper(command[i]);
    }
    char *argument = strtok(NULL, " ");
    if (!strcmp(command, "AUTH")) {
        protocol_client_write_response(client, 504, "Unknown instruction");
        return;
    } else if (!strcmp(command, "USER")) {
        if (!FTP_USER(client, argument)) {
            return;
        }
    } else if (!strcmp(command, "PASS")) {
        if (!FTP_PASS(client, argument)) {
            return;
        }
    } else if (!strcmp(command, "PWD")) {
        if (!FTP_PWD(client, argument)) {
            return;
        }
    } else if (!strcmp(command, "CWD")) {
        if (!FTP_CWD(client, argument)) {
            return;
        }
    } else if (!strcmp(command, "TYPE")) {
        if (!FTP_TYPE(client, argument)) {
            return;
        }
    }
    }
    HANDLE_COMMAND(USER)
    HANDLE_COMMAND(PASS)
    HANDLE_COMMAND(PWD)
    HANDLE_COMMAND(CWD)
    HANDLE_COMMAND(TYPE)

    protocol_client_write_response(client, 504, "State machine failed");
}

void protocol_client_write_response(struct client_data *client, int code, const char *data) {
    client->net_state = NEED_SEND;
    sprintf(client->cmd_send, "%d %s\r\n", code, data);
}
