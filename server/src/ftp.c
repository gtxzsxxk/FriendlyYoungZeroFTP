//
// Created by hanyuan on 2024/10/10.
//

#include "ftp.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

int FTP_USER(struct client_data *client, char *argument) {
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

int FTP_PASS(struct client_data *client, char *argument) {
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

int FTP_PWD(struct client_data *client, char *argument) {
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

int FTP_TYPE(struct client_data *client, char *argument) {
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
