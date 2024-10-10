//
// Created by hanyuan on 2024/10/10.
//

#include "pasv_channel.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <stdlib.h>
#include <poll.h>
#include <time.h>
#include <semaphore.h>
#include "logger.h"
#include "protocol.h"

struct pasv_client_data {
    int port;
    int pasv_server_fd;
    int pasv_client_fd;
    int server_nfds;
    int client_nfds;
    const char *data_to_send;
    size_t send_len;
    pthread_mutex_t lock;
    char recv_buffer[1024];

    enum net_state_machine state_machine;
};

struct pasv_client_data pasv_clients[MAX_CLIENTS];
static struct pollfd fds[MAX_CLIENTS];
static int nfds = 0;

int pipe_fd[2];

static pthread_t pasv_thread_ptr;

static void set_fd_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int pasv_client_new(int *port) {
    int sockfd;
    struct sockaddr_in serv_addr;
    int attempt;

    struct pasv_client_data *client = NULL;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (pasv_clients[i].port == 0) {
            client = &pasv_clients[i];
            break;
        }
    }
    if (!client) {
        logger_err("%s", "No enough space for new pasv clients.");
        return -1;
    }

    srand(time(NULL) + getpid());
    for (attempt = 0; attempt < MAX_ATTEMPTS; attempt++) {
        sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sockfd < 0) {
            logger_err("%s", "Failed to create socket for pasv.");
            return -1;
        }
        set_fd_nonblocking(sockfd);
        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        *port = PORT_RANGE_START + rand() % (PORT_RANGE_END - PORT_RANGE_START + 1);
        serv_addr.sin_port = htons(*port);
        if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) == 0) {
            listen(sockfd, 2);
            client->port = *port;
            client->pasv_server_fd = sockfd;
            client->data_to_send = NULL;
            client->send_len = 0;
            fds[nfds].fd = client->pasv_server_fd;
            fds[nfds].events = POLLIN;
            client->server_nfds = nfds++;
            return 0;
        } else {
            close(sockfd);
        }
    }

    logger_err("Failed to find available port for pasv xmit after %d tries", MAX_ATTEMPTS);
    return -1;
}

int pasv_send_data(int port, const char *data, size_t len) {
    struct pasv_client_data *client = NULL;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (pasv_clients[i].port == port) {
            client = &pasv_clients[i];
            break;
        }
    }
    if (!client) {
        logger_err("No such a client with port %d", port);
        return -1;
    }
    pthread_mutex_lock(&client->lock);
    client->data_to_send = data;
    client->send_len = len;
    client->state_machine = NEED_SEND;
    logger_info("client %p", client);
    write(pipe_fd[1], &client, sizeof(&client));
    pthread_mutex_unlock(&client->lock);
    return 0;
}

static void close_connection(struct pasv_client_data *client) {
    close(client->pasv_server_fd);
    close(client->pasv_client_fd);
    for (int k = client->client_nfds + 1; k < nfds; k++) {
        fds[k - 1] = fds[k];
    }
    for (int k = client->server_nfds + 1; k < nfds; k++) {
        fds[k - 1] = fds[k];
    }
    nfds -= 2;
    if (client->data_to_send) {
        free((void *) client->data_to_send);
    }

    memset(client, 0, sizeof(struct pasv_client_data));
}

static void *pasv_thread(void *args) {
    struct pasv_client_data *client;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        pthread_mutex_init(&(pasv_clients[i].lock), NULL);
    }

    if (pipe(pipe_fd) == -1) {
        logger_err("pasv pipe failed");
        return NULL;
    }

    set_fd_nonblocking(pipe_fd[0]);
    set_fd_nonblocking(pipe_fd[1]);

    fds[nfds].fd = pipe_fd[0];
    fds[nfds].events = POLLIN;
    nfds++;

    while (1) {
        int poll_cnt = poll(fds, nfds, -1);
        if (poll_cnt == -1) {
            logger_err("pasv poll failed");
            return NULL;
        }

        for (int i = 0; i < nfds; i++) {
            client = NULL;
            if (fds[i].revents & POLLIN) {
                if (fds[i].fd == pipe_fd[0]) {
                    /* 需要传输数据 */
                    logger_info("Need xmit data");
                    read(pipe_fd[0], &client, sizeof(&client));
                    if (client->pasv_client_fd) {
                        fds[client->pasv_client_fd].events |= POLLOUT;
                    }
                } else {
                    int client_is_xmitting = 0;
                    for (int c = 0; c < MAX_CLIENTS; c++) {
                        if (pasv_clients[c].pasv_server_fd == fds[i].fd) {
                            client = &pasv_clients[c];
                            break;
                        }
                    }
                    if (!client) {
                        for (int c = 0; c < MAX_CLIENTS; c++) {
                            if (pasv_clients[c].pasv_client_fd == fds[i].fd) {
                                client_is_xmitting = 1;
                                client = &pasv_clients[c];
                                break;
                            }
                        }
                        if (!client) {
                            logger_err("This connection does not have a corresponding pasv_client in list.");
                            return NULL;
                        }
                    }
                    pthread_mutex_lock(&client->lock);
                    if (!client_is_xmitting) {
                        client->pasv_client_fd = accept(fds[i].fd, (struct sockaddr *) &client_addr, &client_len);
                        logger_info("accepted %p", client);
                        if (client->pasv_client_fd <= 0) {
                            logger_err("Failed to accept connection");
                            return NULL;
                        }
                        set_fd_nonblocking(client->pasv_client_fd);
                        fds[nfds].fd = client->pasv_client_fd;
                        fds[nfds].events = POLLIN;
                        client->client_nfds = nfds++;

                        if (client->state_machine == NEED_SEND) {
                            logger_info("accepted and need pollout %d %d %d", fds[client->client_nfds].revents,
                                        client->client_nfds, i);
                            fds[client->client_nfds].events |= POLLOUT;
                        }
                    } else {
                        /* 用户正在传输来数据 */
                        int rev_cnt = recv(fds[i].fd, client->recv_buffer, 512, 0);
                        if (!rev_cnt) {
                            close_connection(client);
                        } else {
                            /* receiving data */

                        }
                    }
                    pthread_mutex_unlock(&client->lock);
                }
            } else if (fds[i].revents & POLLOUT) {
                for (int c = 0; c < MAX_CLIENTS; c++) {
                    if (pasv_clients[c].pasv_client_fd == fds[i].fd) {
                        client = &pasv_clients[c];
                        break;
                    }
                }
                if (!client) {
                    /* TODO: 关闭连接，释放资源 */
                    logger_err("Cannot find the pasv client with fd %d", fds[i].fd);
                    continue;
                }
                pthread_mutex_lock(&client->lock);
                if (client->state_machine == NEED_SEND) {
                    ssize_t sent = send(fds[i].fd, client->data_to_send, client->send_len, 0);
                    free((void *) client->data_to_send);
                    client->data_to_send = NULL;
                    logger_info("send %d bytes data to the pasv client with fd %d", sent, fds[i].fd);
                    if (sent < 0) {
                        logger_err("Cannot send data to the pasv client with fd %d", fds[i].fd);
                    }
                    /* 恢复 POLLIN */
                    fds[i].events = POLLIN;
                    client->state_machine = IDLE;
                } else {
                    logger_err("The pasv client with fd %d should not send by get POLLOUT", fds[i].fd);
                    continue;
                }
                /* 传输结束后，关闭连接 */
                close_connection(client);
                pthread_mutex_unlock(&client->lock);
            }
        }
    }
    return NULL;
}

void pasv_start(void) {
    pthread_create(&pasv_thread_ptr, NULL, pasv_thread, NULL);
}
