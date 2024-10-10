//
// Created by hanyuan on 2024/10/10.
//

#ifndef SERVER_FTP_H
#define SERVER_FTP_H

#include "protocol.h"

#define FTP_FUNC_DEFINE(cmd)    int FTP_##cmd(struct client_data *client, char *argument)

FTP_FUNC_DEFINE(USER);

FTP_FUNC_DEFINE(PASS);

FTP_FUNC_DEFINE(PWD);

int FTP_TYPE(struct client_data *client, char *argument);

FTP_FUNC_DEFINE(TYPE);

#endif //SERVER_FTP_H
