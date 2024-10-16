#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "listener.h"
#include "logger.h"
#include "protocol.h"
#include "filesystem.h"

void usage(int argc, char **argv) {
    printf("%s", "server error: bad arguments\n\n");
    for (int i = 0; i < argc; i++) {
        printf("Unexpected parameter %d: %s\r\n", i, argv[i]);
    }
    printf("%s", "\r\nUsage:\r\n");
    printf("%s", "server -port 11341 -root /root/ftp/\r\n");
}

int main(int argc, char **argv) {
    int assigned_port = 0, assigned_root = 0;
    int port = 0;
    char root_path[256];
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-port")) {
            if (i + 1 < argc) {
                assigned_port = 1;
                port = atoi(argv[i + 1]);
                if (port == 0) {
                    printf("server error: illegal port %d\n", port);
                    return 1;
                }
                i++;
            } else {
                usage(argc, argv);
                return 1;
            }
        } else if (!strcmp(argv[i], "-root")) {
            if (i + 1 < argc) {
                assigned_root = 1;
                strcpy(root_path, argv[i + 1]);
                i++;
            } else {
                usage(argc, argv);
                return 1;
            }
        } else {
            usage(argc, argv);
            return 1;
        }
    }
    if (!assigned_port && !assigned_root) {
        port = 21;
        strcpy(root_path, "/tmp");
    } else if (!assigned_port || !assigned_root) {
        usage(argc, argv);
        return 1;
    }

    /* 获得绝对路径 */
    if (root_path[0] != '/') {
        char pwd[256];
        if (!getcwd(pwd, 256)) {
            logger_err("root filesystem cwd failed %s", root_path);
        }
        const char *abs_path = fs_path_join(pwd, root_path);
        strcpy(root_path, abs_path);
        free((void *) abs_path);
    }

    logger_init();

    logger_info("listening on port %d", port);
    logger_info("root filesystem %s", root_path);

    strcpy(service_root, root_path);
    return start_listen(port);
}
