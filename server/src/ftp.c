//
// Created by hanyuan on 2024/10/10.
//

#include "ftp.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "filesystem.h"
#include "pasv_channel.h"
#include "listener.h"

FTP_FUNC_DEFINE(USER) {
    if (client->ftp_state == NEED_USERNAME) {
        strcpy(client->username, argument);
        if (!strtok(NULL, " ")) {
            client->ftp_state = NEED_PASSWORD;
            protocol_client_write_response(client, 331, "Password is required");
            return 0;
        }
    }
    return 1;
}

FTP_FUNC_DEFINE(PASS) {
    if (client->ftp_state == NEED_PASSWORD) {
        strcpy(client->password, argument);
        if (!strtok(NULL, " ")) {
            client->ftp_state = LOGGED_IN;
            protocol_client_write_welcome_message(client, "Welcome to Friendly Young Zero FTP Server!\r\n"
                                                          "This is the welcome message.\r\n"
                                                          "Welcome message is this.\r\n");
            return 0;
        }
    }
    return 1;
}

FTP_FUNC_DEFINE(PWD) {
    if (client->ftp_state == LOGGED_IN) {
        if (!argument) {
            /* TODO: 用macro定义所有code */
            char *resp = malloc(300);
            sprintf(resp, "\"%s\" is current directory", client->cwd);
            protocol_client_write_response(client, 257, resp);
            free(resp);
            return 0;
        }
    }

    return 1;
}

FTP_FUNC_DEFINE(CWD) {
    if (client->ftp_state == LOGGED_IN) {
        if (argument) {
            const char *fullpath = fs_path_join(service_root, argument);
            if (fs_directory_exists(fullpath)) {
                strcpy(client->cwd, argument);
                protocol_client_write_response(client, 250, "Okay.");
            } else {
                protocol_client_write_response(client, 550, "No such file or directory.");
            }
            free((void *) fullpath);
            return 0;
        }
    }
    return 1;
}

FTP_FUNC_DEFINE(TYPE) {
    if (client->ftp_state == LOGGED_IN) {
        if (argument) {
            if (!strcmp(argument, "A")) {
                client->data_type = ASCII;
                protocol_client_write_response(client, 200, "Type set to Ascii");
                return 0;
            } else if (!strcmp(argument, "I")) {
                client->data_type = BINARY;
                protocol_client_write_response(client, 200, "Type set to I");
                return 0;
            } else if (!strcmp(argument, "L")) {
                client->data_type = BINARY;
                protocol_client_write_response(client, 200, "Type set to L");
                return 0;
            }
        }
    }

    return 1;
}

FTP_FUNC_DEFINE(PASV) {
    if (client->ftp_state == LOGGED_IN) {
        if (!argument) {
            client->conn_type = PASV;
            if (!pasv_client_new(&client->pasv_port)) {
                char *resp = malloc(128);
                sprintf(resp, "Entering Passive Mode (%d,%d,%d,%d,%d,%d)",
                        (load_ip_addr >> 24) & 0xff,
                        (load_ip_addr >> 16) & 0xff,
                        (load_ip_addr >> 8) & 0xff,
                        load_ip_addr & 0xff,
                        client->pasv_port / 256,
                        client->pasv_port % 256);
                protocol_client_write_response(client, 227, resp);
                free(resp);
            } else {
                protocol_client_write_response(client, 434, "No available port assigned for this PASV.");
            }
            return 0;
        }
    }

    return 1;
}
