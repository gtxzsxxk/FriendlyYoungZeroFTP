#include "listener.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <poll.h>
#include "logger.h"
#include "protocol.h"
#include "pasv_channel.h"

struct sockaddr_in local_addr = {0};
uint32_t load_ip_addr = 0;
socklen_t local_len = sizeof(local_addr);

int pasv_send_ctrl_pipe_fd[2];

static struct pollfd fds[MAX_CLIENTS];
static int nfds = 0;

static void set_fd_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static int push_new_fd(struct pollfd fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!fds[i].fd) {
            fds[i] = fd;
            nfds++;
            return i;
        }
    }
    return -1;
}

static void close_fd_connection(int index) {
    close(fds[index].fd);
    fds[index].fd = 0;
    fds[index].events = 0;
    fds[index].revents = 0;
    nfds--;
}

static void close_client_connection(struct client_data *client) {
    logger_info("client disconnected %s:%d",
                inet_ntoa(client->addr.sin_addr),
                ntohs(client->addr.sin_port));
    close_fd_connection(client->nfds);
    protocol_client_free(client->sock_fd);
}

int start_listen(int port) {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    struct client_data *client;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        logger_err("Error socket(): %s(%d)\n", strerror(errno), errno);
        return 1;
    }
    set_fd_nonblocking(server_fd);

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1) {
        logger_err("Error bind(): %s(%d)\n", strerror(errno), errno);
        return 1;
    }
    listen(server_fd, 5);

    /* 初始化管道 */
    if (pipe(pasv_send_ctrl_pipe_fd) == -1) {
        logger_err("ctrl pipe failed");
        return 1;
    }

    set_fd_nonblocking(pasv_send_ctrl_pipe_fd[0]);
    set_fd_nonblocking(pasv_send_ctrl_pipe_fd[1]);

    /* 初始化 poll 结构 */
    fds[nfds].fd = server_fd;
    fds[nfds].events = POLLIN;
    nfds++;
    fds[nfds].fd = pasv_send_ctrl_pipe_fd[0];
    fds[nfds].events = POLLIN;
    nfds++;

    /* 启动被动模式的监听 */
    pasv_start();

    while (1) {
        int poll_cnt = poll(fds, nfds, -1);
        if (poll_cnt == -1) {
            logger_err("poll failed");
            return 1;
        }

        for (int i = 0; i < nfds; i++) {
            client = NULL;
            if (fds[i].revents & POLLIN) {
                if (fds[i].fd == server_fd) {
                    /* 新的连接 */
                    client_fd = accept(server_fd, (struct sockaddr *) &client_addr, &client_len);
                    if (client_fd >= 0) {
                        getsockname(client_fd, (struct sockaddr *) &local_addr, &local_len);
                        load_ip_addr = ntohl(local_addr.sin_addr.s_addr);
                        set_fd_nonblocking(client_fd);
                        int flag = 1;
                        if (setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(flag)) == -1) {
                            logger_err("Failed to set up TCP_NODELAY control");
                            continue;
                        }
                        /* 加入 poll */
                        struct pollfd fd = {
                                .fd = client_fd,
                                .events = POLLIN,
                        };
                        if (nfds == MAX_CLIENTS) {
                            logger_err("Clients number exceeded!");
                            close(client_fd);
                            continue;
                        }
                        client = protocol_client_init(client_fd, push_new_fd(fd));
                        client->addr = client_addr;
                        logger_info("new client connected %s:%d", inet_ntoa(client_addr.sin_addr),
                                    ntohs(client_addr.sin_port));
                    }
                } else if (fds[i].fd == pasv_send_ctrl_pipe_fd[0]) {
                    read(pasv_send_ctrl_pipe_fd[0], &client, sizeof(&client));
                    if (client->sock_fd) {
                        fds[client->nfds].events |= POLLOUT;
                    }
                } else {
                    /* 处理 client 传输过来的命令 */
                    client = protocol_client_by_fd(fds[i].fd);
                    if (!client) {
                        close_fd_connection(i);
                        i = -1;
                        continue;
                    }
                    int rev_cnt = recv(fds[client->nfds].fd, client->cmd_request_buffer + client->recv_ptr, 512, 0);
                    if (rev_cnt > 0) {
                        client->recv_ptr += rev_cnt;
                        int crlf = 0, find_crlf = 0;
                        for (; crlf < client->recv_ptr - 1; crlf++) {
                            if (client->cmd_request_buffer[crlf] == '\r' &&
                                client->cmd_request_buffer[crlf + 1] == '\n') {
                                crlf++;
                                find_crlf = 1;
                                break;
                            }
                        }
                        if (find_crlf) {
                            memcpy(client->cmd_request, client->cmd_request_buffer, crlf + 1);
                            /* 直接去掉接收到的命令里的\r\n */
                            client->cmd_request[crlf - 1] = '\0';
                            memcpy(client->cmd_request_buffer, client->cmd_request_buffer + crlf + 1,
                                   client->recv_ptr - crlf - 1);
                            client->recv_ptr -= crlf + 1;
                            protocol_on_recv(client->sock_fd);
                        }
                    } else {
                        close_client_connection(client);
                        i = -1;
                        continue;
                    }
                }

                if (client && client->sock_fd) {
                    if (client->net_state == NEED_SEND) {
                        fds[client->nfds].events = POLLOUT;
                    }
                }
            } else if (fds[i].revents & POLLOUT) {
                /* 发送数据包 */
                client = protocol_client_by_fd(fds[i].fd);
                if (!client) {
                    if (i > 1) {
                        close_fd_connection(i);
                        i = -1;
                    }
                    continue;
                }
                char *msg = client->cmd_send;
                size_t len = strlen(msg);
                ssize_t sent = send(fds[i].fd, msg, len, 0);
                pthread_mutex_unlock(&client->net_lock);
                if (sent < 0) {
                    logger_err("Cannot send data to the client with fd %d", fds[i].fd);
                }
                /* 恢复 POLLIN */
                fds[i].events = POLLIN;

                client->net_state = IDLE;
            } else if (fds[i].revents & POLLNVAL) {
                if (i > 1) {
                    close_fd_connection(i);
                    i = -1;
                    continue;
                }
            }
        }
    }

    return 0;
}
