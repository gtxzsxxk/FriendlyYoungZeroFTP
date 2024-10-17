//
// Created by hanyuan on 2024/10/10.
//

#include "port_channel.h"
#include <sys/socket.h>
#include <sys/errno.h>

#if __APPLE__

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>

#else

#include <sys/sendfile.h>

#endif

#include <netinet/in.h>
#include <netinet/tcp.h>

#define _GNU_SOURCE

#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <signal.h>
#include <poll.h>
#include "logger.h"
#include "listener.h"
#include "filesystem.h"

#if !defined(__APPLE__)
#ifndef SPLICE_F_MOVE
#define SPLICE_F_MOVE   0

ssize_t splice(int fd_in, loff_t *off_in, int fd_out,
               loff_t *off_out, size_t len, unsigned int flags);

#endif
#endif

struct port_client_data {
    struct sockaddr_in target_addr;
    int sock_fd;
    int sock_nfds;
    struct pollfd poll_client_fd;
    const char *data_to_send;
    char file_to_send[256];
    int file_fd;
    off_t send_offset;
    size_t send_len;
    pthread_mutex_t lock;
    int write_pipe_fd[2];
    char ctrl_send_buffer[512];
#if defined(__APPLE__)
    char recv_buffer[4096];
#endif
    struct client_data *ctrl_client;

    enum net_state_machine state_machine;
};

struct port_client_data port_clients[MAX_CLIENTS];
static struct pollfd fds[MAX_CLIENTS];
static int fd_most_tail = 0;

static int ctrl_send_data_pipe_fd[2];

static pthread_t port_thread_ptr;

static void set_fd_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static int push_new_fd(struct pollfd fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (fds[i].fd == -1) {
            fds[i] = fd;
            if (i >= fd_most_tail) {
                fd_most_tail++;
            }
            return i;
        }
    }
    return -1;
}

int port_client_new(struct sockaddr_in target_addr) {
    int sockfd;

    struct port_client_data *client = NULL;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (port_clients[i].target_addr.sin_addr.s_addr == 0) {
            client = &port_clients[i];
            break;
        }
    }
    if (!client) {
        logger_err("%s", "No enough space for new port clients.");
        return -1;
    }

    pthread_mutex_init(&client->lock, NULL);

    sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd < 0) {
        logger_err("%s", "Failed to create socket for port.");
        pthread_mutex_destroy(&client->lock);
        return -1;
    }
    set_fd_nonblocking(sockfd);
    int conn_ret = connect(sockfd, (struct sockaddr *) &target_addr, sizeof(target_addr));
    if (conn_ret < 0 && errno == EINPROGRESS) {
        client->target_addr = target_addr;
        client->sock_fd = sockfd;
        client->data_to_send = NULL;
        client->send_offset = 0;
        client->send_len = 0;
        client->write_pipe_fd[0] = -1;
        client->write_pipe_fd[1] = -1;
        client->file_fd = -1;
        struct pollfd tmp_fd = {
                .fd = client->sock_fd,
                .events = POLLIN,
        };
        client->poll_client_fd = tmp_fd;
        client->state_machine = NEW_CLIENT;
        int ret = write(ctrl_send_data_pipe_fd[1], &client, sizeof(&client));
        if (ret <= 0) {
            return 1;
        }
        return 0;
    } else {
        close(sockfd);
    }

    logger_err("Failed to find available port for port xmit after %d tries", MAX_ATTEMPTS);
    pthread_mutex_destroy(&client->lock);
    return -1;
}

int port_send_set_rest(struct client_data *ctrl_client, off_t offset) {
    struct port_client_data *client = NULL;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (port_clients[i].target_addr.sin_addr.s_addr == ctrl_client->port_target_addr.sin_addr.s_addr &&
            port_clients[i].target_addr.sin_port == ctrl_client->port_target_addr.sin_port) {
            client = &port_clients[i];
            break;
        }
    }
    if (!client) {
        logger_err("No such a PORT client");
        return -1;
    }
    pthread_mutex_lock(&client->lock);
    client->send_offset = offset;
    pthread_mutex_unlock(&client->lock);
    return 0;
}

int port_send_data(struct client_data *ctrl_client,
                   const char *data,
                   size_t len,
                   const char *end_msg) {
    struct port_client_data *client = NULL;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (port_clients[i].target_addr.sin_addr.s_addr == ctrl_client->port_target_addr.sin_addr.s_addr &&
            port_clients[i].target_addr.sin_port == ctrl_client->port_target_addr.sin_port) {
            client = &port_clients[i];
            break;
        }
    }
    if (!client) {
        logger_err("No such a PORT client");
        return -1;
    }
    pthread_mutex_lock(&client->lock);
    client->data_to_send = data;
    client->send_len = len;
    client->state_machine = NEED_SEND;
    strcpy(client->ctrl_send_buffer, end_msg);
    client->ctrl_client = ctrl_client;
    pthread_mutex_unlock(&client->lock);
    int ret = write(ctrl_send_data_pipe_fd[1], &client, sizeof(&client));
    if (ret <= 0) {
        return 1;
    }
    return 0;
}

int port_sendfile(struct client_data *ctrl_client,
                  const char *path,
                  const char *end_msg) {
    struct port_client_data *client = NULL;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (port_clients[i].target_addr.sin_addr.s_addr == ctrl_client->port_target_addr.sin_addr.s_addr &&
            port_clients[i].target_addr.sin_port == ctrl_client->port_target_addr.sin_port) {
            client = &port_clients[i];
            break;
        }
    }
    if (!client) {
        logger_err("No such a PORT client");
        return -1;
    }
    pthread_mutex_lock(&client->lock);
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
    pthread_mutex_unlock(&client->lock);
    int ret = write(ctrl_send_data_pipe_fd[1], &client, sizeof(&client));
    if (ret <= 0) {
        return 1;
    }
    return 0;
}

int port_recvfile(struct client_data *ctrl_client,
                  const char *path,
                  const char *end_msg) {
    struct port_client_data *client = NULL;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (port_clients[i].target_addr.sin_addr.s_addr == ctrl_client->port_target_addr.sin_addr.s_addr &&
            port_clients[i].target_addr.sin_port == ctrl_client->port_target_addr.sin_port) {
            client = &port_clients[i];
            break;
        }
    }
    if (!client) {
        logger_err("No such a PORT client");
        return -1;
    }
    pthread_mutex_lock(&client->lock);
    client->data_to_send = NULL;
    client->file_fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (client->file_fd < 0) {
        return 1;
    }
    if (pipe(client->write_pipe_fd) < 0) {
        close(client->file_fd);
        return 1;
    }
    if (client->send_offset > 0) {
        if (lseek(client->file_fd, client->send_offset, SEEK_SET) == -1) {
            logger_err("Failed to set REST file offset.");
            return 1;
        }
    }
    client->send_len = 0;
    client->state_machine = NEED_RECV;
    if (ctrl_client) {
        strcpy(client->ctrl_send_buffer, end_msg);
        client->ctrl_client = ctrl_client;
    } else {
        client->ctrl_client = NULL;
    }
    pthread_mutex_unlock(&client->lock);
    int ret = write(ctrl_send_data_pipe_fd[1], &client, sizeof(&client));
    if (ret <= 0) {
        return 1;
    }
    return 0;
}

int port_close_connection(struct client_data *ctrl_client) {
    struct port_client_data *client = NULL;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (port_clients[i].target_addr.sin_addr.s_addr == ctrl_client->port_target_addr.sin_addr.s_addr &&
            port_clients[i].target_addr.sin_port == ctrl_client->port_target_addr.sin_port) {
            client = &port_clients[i];
            break;
        }
    }
    if (!client) {
        logger_err("No such a PORT client");
        return -1;
    }
    pthread_mutex_lock(&client->lock);
    client->state_machine = NEED_QUIT;
    pthread_mutex_unlock(&client->lock);
    int ret = write(ctrl_send_data_pipe_fd[1], &client, sizeof(&client));
    if (ret <= 0) {
        return 1;
    }
    return 0;
}

static void close_connection(struct port_client_data *client) {
    close(client->sock_fd);
    fds[client->sock_nfds].fd = -1;
    fds[client->sock_nfds].events = 0;
    if (client->data_to_send) {
        free((void *) client->data_to_send);
    }
    if (client->file_fd > 0) {
        close(client->file_fd);
    }
    if (client->write_pipe_fd[0] > 0) {
        close(client->write_pipe_fd[0]);
    }
    if (client->write_pipe_fd[1] > 0) {
        close(client->write_pipe_fd[1]);
    }

    pthread_mutex_unlock(&client->lock);
    pthread_mutex_destroy(&client->lock);
    memset(client, 0, sizeof(struct port_client_data));
}

static void *port_thread(void *args) {
    struct port_client_data *client;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        fds[i].fd = -1;
    }

    if (pipe(ctrl_send_data_pipe_fd) == -1) {
        logger_err("port pipe failed");
        return NULL;
    }

    set_fd_nonblocking(ctrl_send_data_pipe_fd[0]);
    set_fd_nonblocking(ctrl_send_data_pipe_fd[1]);

    fds[fd_most_tail].fd = ctrl_send_data_pipe_fd[0];
    fds[fd_most_tail].events = POLLIN;
    fd_most_tail++;

    signal(SIGPIPE, SIG_IGN);

    while (1) {
        int poll_cnt = poll(fds, fd_most_tail, -1);
        if (poll_cnt == -1) {
            logger_err("port poll failed");
            return NULL;
        }
        for (int i = 0; i < fd_most_tail; i++) {
            if (fds[i].fd == -1) {
                continue;
            }
            client = NULL;
            if (fds[i].revents & POLLIN) {
                if (fds[i].fd == ctrl_send_data_pipe_fd[0]) {
                    /* 需要传输数据 */
                    int ret = read(ctrl_send_data_pipe_fd[0], &client, sizeof(&client));
                    if (ret <= 0) {
                        continue;
                    }
                    if (client->state_machine == NEW_CLIENT) {
                        client->sock_nfds = push_new_fd(client->poll_client_fd);
                        if (client->sock_nfds == -1) {
                            logger_err("Client numbers for port exceeded!");
                            pthread_mutex_destroy(&client->lock);
                            continue;
                        }
                        client->state_machine = IDLE;
                    } else if (client->sock_fd) {
                        if (client->state_machine == NEED_SEND) {
                            fds[client->sock_nfds].events |= POLLOUT | POLLERR | POLLHUP | POLLNVAL;
                        } else if (client->state_machine == NEED_RECV) {
                            fds[client->sock_nfds].events = POLLIN | POLLERR | POLLHUP | POLLNVAL;
                        } else if (client->state_machine == NEED_QUIT) {
                            close_connection(client);
                            i = -1;
                            continue;
                        }
                    }
                } else {
                    for (int c = 0; c < MAX_CLIENTS; c++) {
                        if (port_clients[c].sock_fd == fds[i].fd) {
                            client = &port_clients[c];
                            break;
                        }
                    }
                    if (!client) {
                        logger_err("This connection does not have a corresponding port_client in list.");
                        return NULL;
                    }
                    pthread_mutex_lock(&client->lock);
                    /* 用户正在传输来数据 */
#if !defined(__APPLE__)
                    ssize_t rev_cnt = splice(fds[i].fd, NULL, client->write_pipe_fd[1], NULL, 4096,
                                             SPLICE_F_MOVE);
#else
                    ssize_t rev_cnt = read(fds[i].fd, client->recv_buffer, 4096);
#endif
                    if (rev_cnt == 0) {
                        /* 连接被关闭 */
                        logger_info("Pasv client with fd %d ended uploading", fds[i].fd);
                        /* Callback */
                        if (client->ctrl_client) {
                            pthread_mutex_lock(&client->ctrl_client->net_lock);
                            /* 发送完毕 */
                            client->ctrl_client->conn_type = NOT_SPECIFIED;
                            strcpy(client->ctrl_client->cmd_send, client->ctrl_send_buffer);
                            client->ctrl_client->net_state = NEED_SEND;
                            int ret = write(data_send_ctrl_pipe_fd[1], &client->ctrl_client,
                                            sizeof(&client->ctrl_client));
                            if (ret <= 0) {
                                continue;
                            }
                        }
                        close_connection(client);
                        i = -1;
                        continue;
                    }
                    if (rev_cnt < 0) {
                        logger_err("Pasv client with fd %d cannot use splice", fds[i].fd);
                        close_connection(client);
                        i = -1;
                        continue;
                    }
                    if (client->state_machine == NEED_RECV) {
#if !defined(__APPLE__)
                        ssize_t wr_cnt = splice(client->write_pipe_fd[0], NULL, client->file_fd, NULL, rev_cnt,
                                                SPLICE_F_MOVE);
#else
                        ssize_t wr_cnt = write(client->file_fd, client->recv_buffer, rev_cnt);
#endif
                        if (wr_cnt < 0) {
                            logger_err("Pasv client with fd %d cannot use splice to write file", fds[i].fd);
                            close_connection(client);
                            i = -1;
                            continue;
                        }
                    } else {
                        logger_err("Pasv client with fd %d is not in a send state", fds[i].fd);
                        close_connection(client);
                        i = -1;
                        continue;
                    }

                    pthread_mutex_unlock(&client->lock);
                }
            } else if (fds[i].revents & POLLOUT) {
                for (int c = 0; c < MAX_CLIENTS; c++) {
                    if (port_clients[c].sock_fd == fds[i].fd) {
                        client = &port_clients[c];
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
                        logger_err("Cannot send data to the port client with fd %d", fds[i].fd);
                        close_connection(client);
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
                            /* 发送完毕 */
                            client->ctrl_client->conn_type = NOT_SPECIFIED;
                            strcpy(client->ctrl_client->cmd_send, client->ctrl_send_buffer);
                            client->ctrl_client->net_state = NEED_SEND;
                            int ret = write(data_send_ctrl_pipe_fd[1], &client->ctrl_client,
                                            sizeof(&client->ctrl_client));
                            if (ret <= 0) {
                                continue;
                            }
                        }
                        /* 传输结束后，关闭连接 */
                        close_connection(client);
                        i = -1;
                        continue;
                    } else {
                        fds[i].events = POLLOUT | POLLERR | POLLHUP | POLLNVAL;
                    }
                }
                pthread_mutex_unlock(&client->lock);
            } else if (fds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                for (int c = 0; c < MAX_CLIENTS; c++) {
                    if (port_clients[c].sock_fd == fds[i].fd) {
                        client = &port_clients[c];
                        break;
                    }
                }
                if (!client) {
                    logger_err("Critical error on socket %d with poll error.", fds[i].fd);
                    close(fds[i].fd);
                    continue;
                }
                close_connection(client);
                i = -1;
                continue;
            }
        }
    }

    return NULL;
}

void port_start(void) {
    pthread_create(&port_thread_ptr, NULL, port_thread, NULL);
}
