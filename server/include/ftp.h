//
// Created by hanyuan on 2024/10/10.
//

#ifndef SERVER_FTP_H
#define SERVER_FTP_H

#include "protocol.h"

int FTP_USER(struct client_data *client, char *argument);

int FTP_PASS(struct client_data *client, char *argument);

int FTP_PWD(struct client_data *client, char *argument);

int FTP_TYPE(struct client_data *client, char *argument);

#endif //SERVER_FTP_H
