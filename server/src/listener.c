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

#define MAX_CLIENTS 100

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
            if (fds[i].revents & POLLIN) {
                if (fds[i].fd == server_fd) {
                    /* 新的连接 */
                    client_fd = accept(server_fd, (struct sockaddr *) &client_addr, &client_len);
                    if (client_fd >= 0) {
                        set_fd_nonblocking(client_fd);
                        /* 加入 poll */
                        fds[nfds].fd = client_fd;
                        fds[nfds].events = POLLIN;
                        nfds++;
                        logger_info("new client connected %s:%d", inet_ntoa(client_addr.sin_addr),
                                    ntohs(client_addr.sin_port));
                    }
                } else {
                    /* 处理命令 */

                }
            }
        }
    }

    return 0;
}
