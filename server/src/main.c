#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "listener.h"
#include "logger.h"

void usage(void) {
    printf("%s", "server error: bad arguments\n\n");
    printf("%s", "Usage:\n");
    printf("%s", "server -port 11341 -root /root/ftp/\n");
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
                usage();
                return 1;
            }
        } else if (!strcmp(argv[i], "-root")) {
            if (i + 1 < argc) {
                assigned_root = 1;
                strcpy(root_path, argv[i + 1]);
                i++;
            } else {
                usage();
                return 1;
            }
        } else {
            usage();
            return 1;
        }
    }
    if (!assigned_port || !assigned_root) {
        usage();
        return 1;
    }

    logger_info("listening on port %d", port);
    logger_info("root filesystem %s", root_path);

    return listen_blocking(port);
}
