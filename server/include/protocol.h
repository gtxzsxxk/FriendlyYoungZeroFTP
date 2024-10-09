//
// Created by hanyuan on 2024/10/9.
//

#ifndef SERVER_PROTOCOL_H
#define SERVER_PROTOCOL_H

#define MAX_CLIENTS 100

enum ftp_state_machine {
    IDLE,
    NEED_SEND,
};

struct client_data {
    int sock_fd;
    int nfds;
    enum ftp_state_machine state;

    /* 解决粘包，根据CRLF进行切分 */
    int recv_ptr;
    char cmd_request_buffer[512];
    char cmd_request[512];
    char cmd_send[512];
};

struct client_data *protocol_client_init(int fd, int nfds);

void protocol_client_free(int fd);

struct client_data *protocol_client_by_fd(int fd);

#endif //SERVER_PROTOCOL_H
