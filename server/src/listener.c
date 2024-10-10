#include "listener.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <poll.h>
#include "logger.h"
#include "protocol.h"

static void set_fd_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int start_listen(int port) {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    struct pollfd fds[MAX_CLIENTS];
    int nfds = 1;
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

    /* 初始化 poll 结构 */
    fds[0].fd = server_fd;
    fds[0].events = POLLIN;

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
                        set_fd_nonblocking(client_fd);
                        /* 加入 poll */
                        fds[nfds].fd = client_fd;
                        fds[nfds].events = POLLIN;
                        client = protocol_client_init(client_fd, nfds++);
                        logger_info("new client connected %s:%d", inet_ntoa(client_addr.sin_addr),
                                    ntohs(client_addr.sin_port));
                    }
                } else {
                    /* 处理 client 传输过来的命令 */
                    client = protocol_client_by_fd(fds[i].fd);
                    if (!client) {
                        /* TODO: 关闭连接，释放资源 */
                        logger_err("Cannot find the client with fd %d", fds[i].fd);
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
                    } else if (!rev_cnt) {
                        /* close connection */
                        logger_info("client disconnected %s:%d", inet_ntoa(client_addr.sin_addr),
                                    ntohs(client_addr.sin_port));
                        protocol_client_free(fds[client->nfds].fd);
                        close(fds[client->nfds].fd);
                        nfds--;
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
                    /* TODO: 关闭连接，释放资源 */
                    logger_err("Cannot find the client with fd %d", fds[i].fd);
                    continue;
                }
                char *msg = client->cmd_send;
                size_t len = strlen(msg);
                ssize_t sent = send(fds[i].fd, msg, len, 0);
                if (sent < 0) {
                    logger_err("Cannot send data to the client with fd %d", fds[i].fd);
                }
                /* 恢复 POLLIN */
                fds[i].events = POLLIN;

                client->net_state = IDLE;
            }
        }
    }

    return 0;
}
