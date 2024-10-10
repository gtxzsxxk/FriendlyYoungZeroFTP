//
// Created by hanyuan on 2024/10/9.
//

#include "../include/protocol.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
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

static void protocol_client_write_welcome_message(struct client_data *client, const char *data) {
    char *line;
    char *text = malloc(512);
    strcpy(text, data);
    int start = 0;
    for (line = strtok(text, "\r\n"); line; line = strtok(NULL, "\r\n")) {
        sprintf(client->cmd_send + start, "230-%s\r\n", line);
        start = strlen(client->cmd_send);
    }
    client->net_state = NEED_SEND;
    free(text);
}

struct client_data *protocol_client_init(int fd, int nfds) {
    /* 分配一个新的 client_data */
    int index = protocol_client_index_by_fd(fd);
    clients[index].sock_fd = fd;
    clients[index].nfds = nfds;

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
    logger_info("receive data: %s", client->cmd_request);
    char *command;
    command = strtok(client->cmd_request, " ");
    if (!strcmp(command, "AUTH")) {
        protocol_client_write_response(client, 504, "Unknown instruction");
        return;
    } else if (!strcmp(command, "USER")) {
        if (client->ftp_state == NEED_USERNAME) {
            char *argument = strtok(NULL, " ");
            strcpy(client->username, argument);
            if (!strtok(NULL, " ")) {
                client->ftp_state = NEED_PASSWORD;
                protocol_client_write_response(client, 331, "Password is required");
                return;
            }
        }
        goto bad;
    } else if (!strcmp(command, "PASS")) {
        if (client->ftp_state == NEED_PASSWORD) {
            char *argument = strtok(NULL, " ");
            strcpy(client->password, argument);
            if (!strtok(NULL, " ")) {
                client->ftp_state = LOGGED_IN;
                protocol_client_write_welcome_message(client, "Welcome to Friendly Young Zero FTP Server!\r\n"
                                                              "This is the welcome message.\r\n"
                                                              "Welcome message is this.\r\n");
                return;
            }
        }
        goto bad;
    }

    bad:
    protocol_client_write_response(client, 504, "State machine failed");
}

void protocol_client_write_response(struct client_data *client, int code, const char *data) {
    client->net_state = NEED_SEND;
    sprintf(client->cmd_send, "%d %s\r\n", code, data);
}
