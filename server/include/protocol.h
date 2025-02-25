//
// Created by hanyuan on 2024/10/9.
//

#ifndef SERVER_PROTOCOL_H
#define SERVER_PROTOCOL_H

#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>

#define MAX_CLIENTS 20

enum net_state_machine {
    IDLE,
    NEW_CLIENT,
    NEED_SEND,
    NEED_RECV,
    NEED_QUIT
};

enum ftp_state_machine {
    NEED_USERNAME,
    NEED_PASSWORD,
    LOGGED_IN,
};

enum ftp_data_type {
    ASCII,
    BINARY
};

enum ftp_conn_type {
    NOT_SPECIFIED,
    PORT,
    PASV
};

struct client_data {
    int sock_fd;
    struct sockaddr_in addr;

    int nfds;
    enum net_state_machine net_state;
    enum ftp_state_machine ftp_state;

    /* 解决粘包，根据CRLF进行切分 */
    int recv_ptr;
    char cmd_request_buffer[512];
    char cmd_request[512];
    char full_instruction[512];
    char cmd_send[512];
    pthread_mutex_t net_lock;

    char username[20];
    char password[64];

    char cwd[256];

    enum ftp_data_type data_type;
    enum ftp_conn_type conn_type;

    int pasv_port;
    struct sockaddr_in port_target_addr;
};

#define HANDLE_COMMAND(cmd)     else if (!strcmp(command, #cmd)) { \
        if (!FTP_##cmd(client, argument)) { \
            return; \
        } \
    }

extern char service_root[];

struct client_data *protocol_client_init(int fd, int nfds);

void protocol_client_free(int fd);

struct client_data *protocol_client_by_fd(int fd);

void protocol_on_recv(int fd);

void protocol_client_resp_by_state_machine(struct client_data *client, int code, const char *data);

void protocol_client_quit(struct client_data *client);

void protocol_client_write_welcome_message(struct client_data *client, const char *data);

#endif //SERVER_PROTOCOL_H
