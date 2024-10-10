//
// Created by hanyuan on 2024/10/9.
//

#ifndef SERVER_PROTOCOL_H
#define SERVER_PROTOCOL_H

#define MAX_CLIENTS 100

enum net_state_machine {
    IDLE,
    NEED_SEND,
};

enum ftp_state_machine {
    NEED_USERNAME,
    NEED_PASSWORD,
    LOGGED_IN,
};

struct client_data {
    int sock_fd;
    int nfds;
    enum ftp_state_machine state;
    enum net_state_machine net_state;
    enum ftp_state_machine ftp_state;

    /* 解决粘包，根据CRLF进行切分 */
    int recv_ptr;
    char cmd_request_buffer[512];
    char cmd_request[512];
    char cmd_send[512];
};

struct client_data *protocol_client_init(int fd, int nfds);

void protocol_client_free(int fd);

struct client_data *protocol_client_by_fd(int fd);

void protocol_on_recv(int fd);

#endif //SERVER_PROTOCOL_H
