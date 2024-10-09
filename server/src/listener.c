#include "listener.h"
#include <sys/socket.h>
#include <netinet/in.h>
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

int listen_blocking(int port) {
    int server_fd;
    struct sockaddr_in addr;
    struct pollfd fds[MAX_CLIENTS];
    int nfds = 1;

    //创建socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        logger_err("Error socket(): %s(%d)\n", strerror(errno), errno);
        return 1;
    }
    set_fd_nonblocking(server_fd);

    //设置本机的ip和port
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);    //监听"0.0.0.0"

    //将本机的ip和port与socket绑定
    if (bind(server_fd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
        logger_err("Error bind(): %s(%d)\n", strerror(errno), errno);
        return 1;
    }

    return 0;
}
