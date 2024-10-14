//
// Created by hanyuan on 2024/10/10.
//

#include "pasv_channel.h"
#include <sys/socket.h>

#if __APPLE__

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>

#else

#include <sys/sendfile.h>

#endif

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <poll.h>
#include <time.h>
#include "logger.h"
#include "listener.h"
#include "filesystem.h"

struct pasv_client_data {
    int port;
    int pasv_server_fd;
    int pasv_client_fd;
    int server_nfds;
    int client_nfds;
    const char *data_to_send;
    char file_to_send[256];
    int file_fd;
    off_t send_offset;
    size_t send_len;
    pthread_mutex_t lock;
    char recv_buffer[1024];
    char ctrl_send_buffer[512];
    struct client_data *ctrl_client;

    enum net_state_machine state_machine;
};

struct pasv_client_data pasv_clients[MAX_CLIENTS];
static struct pollfd fds[MAX_CLIENTS];
static int fd_most_tail = 0;
pthread_mutex_t fd_lock;

int ctrl_send_data_pipe_fd[2];

static pthread_t pasv_thread_ptr;

static void set_fd_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static int push_new_fd(struct pollfd fd) {
    pthread_mutex_lock(&fd_lock);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (fds[i].fd == -1) {
            fds[i] = fd;
            if (i >= fd_most_tail) {
                fd_most_tail++;
            }
            pthread_mutex_unlock(&fd_lock);
            return i;
        }
    }
    pthread_mutex_unlock(&fd_lock);
    return -1;
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
            struct pollfd fd = {
                    .fd = client->pasv_server_fd,
                    .events = POLLIN,
            };
            client->server_nfds = push_new_fd(fd);
            if (client->server_nfds == -1) {
                logger_err("Client numbers for pasv exceeded!");
                return 1;
            }
            return 0;
        } else {
            close(sockfd);
        }
    }

    logger_err("Failed to find available port for pasv xmit after %d tries", MAX_ATTEMPTS);
    return -1;
}

int pasv_send_data(int port, const char *data, size_t len,
                   struct client_data *ctrl_client,
                   const char *end_msg) {
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
    client->send_offset = 0;
    client->data_to_send = data;
    client->send_len = len;
    client->state_machine = NEED_SEND;
    if (ctrl_client) {
        strcpy(client->ctrl_send_buffer, end_msg);
        client->ctrl_client = ctrl_client;
    } else {
        client->ctrl_client = NULL;
    }
    write(ctrl_send_data_pipe_fd[1], &client, sizeof(&client));
    pthread_mutex_unlock(&client->lock);
    return 0;
}

int pasv_sendfile(int port, const char *path,
                  struct client_data *ctrl_client,
                  const char *end_msg) {
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
    client->send_offset = 0;
    client->data_to_send = NULL;
    strcpy(client->file_to_send, path);
    client->file_fd = -1;
    client->send_len = fs_get_file_size(path);
    client->state_machine = NEED_SEND;
    if (ctrl_client) {
        strcpy(client->ctrl_send_buffer, end_msg);
        client->ctrl_client = ctrl_client;
    } else {
        client->ctrl_client = NULL;
    }
    write(ctrl_send_data_pipe_fd[1], &client, sizeof(&client));
    pthread_mutex_unlock(&client->lock);
    return 0;
}

static void close_connection(struct pasv_client_data *client) {
    close(client->pasv_server_fd);
    close(client->pasv_client_fd);
    fds[client->client_nfds].fd = -1;
    fds[client->client_nfds].events = 0;
    fds[client->server_nfds].fd = -1;
    fds[client->server_nfds].events = 0;
    if (client->data_to_send) {
        free((void *) client->data_to_send);
    }
    if (client->file_fd > 0) {
        close(client->file_fd);
    }

    memset(client, 0, sizeof(struct pasv_client_data));
}

static void *pasv_thread(void *args) {
    struct pasv_client_data *client;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        fds[i].fd = -1;
        pthread_mutex_init(&(pasv_clients[i].lock), NULL);
    }

    pthread_mutex_init(&fd_lock, NULL);

    if (pipe(ctrl_send_data_pipe_fd) == -1) {
        logger_err("pasv pipe failed");
        return NULL;
    }

    set_fd_nonblocking(ctrl_send_data_pipe_fd[0]);
    set_fd_nonblocking(ctrl_send_data_pipe_fd[1]);

    fds[fd_most_tail].fd = ctrl_send_data_pipe_fd[0];
    fds[fd_most_tail].events = POLLIN;
    fd_most_tail++;

    while (1) {
        int poll_cnt = poll(fds, fd_most_tail, -1);
        if (poll_cnt == -1) {
            logger_err("pasv poll failed");
            return NULL;
        }
        pthread_mutex_lock(&fd_lock);
        for (int i = 0; i < fd_most_tail; i++) {
            if (fds[i].fd == -1) {
                continue;
            }
            client = NULL;
            if (fds[i].revents & POLLIN) {
                if (fds[i].fd == ctrl_send_data_pipe_fd[0]) {
                    /* 需要传输数据 */
                    read(ctrl_send_data_pipe_fd[0], &client, sizeof(&client));
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
                        if (client->pasv_client_fd <= 0) {
                            logger_err("Failed to accept connection");
                            return NULL;
                        }
                        set_fd_nonblocking(client->pasv_client_fd);
                        int flag = 1;
                        if (setsockopt(client->pasv_client_fd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag,
                                       sizeof(flag)) == -1) {
                            logger_err("Failed to set up TCP_NODELAY connection");
                            close_connection(client);
                            i = -1;
                            continue;
                        }
                        struct pollfd fd = {
                                .fd = client->pasv_client_fd,
                                .events = POLLIN,
                        };
                        client->client_nfds = push_new_fd(fd);

                        if (client->state_machine == NEED_SEND) {
                            fds[client->client_nfds].events |= POLLOUT;
                        }
                    } else {
                        /* 用户正在传输来数据 */
                        int rev_cnt = recv(fds[i].fd, client->recv_buffer, 512, 0);
                        if (!rev_cnt) {
                            close_connection(client);
                            i = -1;
                            continue;
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
                    continue;
                }
                pthread_mutex_lock(&client->lock);
                if (client->state_machine == NEED_SEND) {
                    ssize_t sent;
                    size_t send_chunk_size = client->send_len - client->send_offset;
                    if (send_chunk_size > MAX_SEND_CHUNK) {
                        send_chunk_size = MAX_SEND_CHUNK;
                    }
                    if (client->data_to_send) {
                        sent = send(fds[i].fd,
                                    client->data_to_send + client->send_offset,
                                    send_chunk_size, 0);
                        client->send_offset += sent;
                    } else {
                        if (client->file_fd <= 0) {
                            client->file_fd = open(client->file_to_send, O_RDONLY);
                        }
#if __APPLE__
                        off_t tmp_sent = (off_t) send_chunk_size;
                        int ret = sendfile(client->file_fd,
                                           fds[i].fd,
                                           client->send_offset,
                                           &tmp_sent,
                                           NULL,
                                           0);
                        if (ret == -1 && !tmp_sent) {
                            sent = -1;
                        } else {
                            sent = tmp_sent;
                            client->send_offset += sent;
                        }
#else
                        sent = sendfile(fds[i].fd,
                                        client->file_fd,
                                        &client->send_offset,
                                        send_chunk_size);
#endif
                    }
                    if (sent < 0) {
                        logger_err("Cannot send data to the pasv client with fd %d", fds[i].fd);
                        close_connection(client);
                        pthread_mutex_unlock(&client->lock);
                        i = -1;
                        continue;
                    }
                    if (client->send_offset == client->send_len) {
                        if (client->data_to_send) {
                            free((void *) client->data_to_send);
                            client->data_to_send = NULL;
                        } else {
                            close(client->file_fd);
                            client->file_fd = -1;
                        }
                        /* Callback */
                        if (client->ctrl_client) {
                            pthread_mutex_lock(&client->ctrl_client->net_lock);
                            strcpy(client->ctrl_client->cmd_send, client->ctrl_send_buffer);
                            client->ctrl_client->net_state = NEED_SEND;
                            write(pasv_send_ctrl_pipe_fd[1], &client->ctrl_client, sizeof(&client->ctrl_client));
                        }
                        /* 传输结束后，关闭连接 */
                        close_connection(client);
                        pthread_mutex_unlock(&client->lock);
                        i = -1;
                        continue;
                    } else {
                        fds[i].events = POLLOUT;
                    }
                } else {
                    logger_err("The pasv client with fd %d should not send by get POLLOUT", fds[i].fd);
                }
                pthread_mutex_unlock(&client->lock);
            }
            pthread_mutex_unlock(&fd_lock);
        }
    }

    pthread_mutex_destroy(&fd_lock);
    return NULL;
}

void pasv_start(void) {
    pthread_create(&pasv_thread_ptr, NULL, pasv_thread, NULL);
}
