//
// Created by hanyuan on 2024/10/10.
//

#include "ftp.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "filesystem.h"
#include "pasv_channel.h"
#include "port_channel.h"
#include "listener.h"
#include "logger.h"

FTP_FUNC_DEFINE(USER) {
    if (client->ftp_state == NEED_USERNAME) {
        strcpy(client->username, argument);
        if (strcmp(client->username, "anonymous") != 0) {
            client->ftp_state = NEED_USERNAME;
            protocol_client_resp_by_state_machine(client, 430, "Only support anonymous user.");
            return 0;
        } else {
            if (!strtok(NULL, " ")) {
                client->ftp_state = NEED_PASSWORD;
                protocol_client_resp_by_state_machine(client, 331, "Password is required.");
                return 0;
            }
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
            char *free_handle;
            const char *rel_path = fs_path_erase(service_root, client->cwd, &free_handle);
            sprintf(resp, "\"%s\" is current directory", rel_path);
            protocol_client_resp_by_state_machine(client, 257, resp);
            free(free_handle);
            free(resp);
            return 0;
        }
    }

    return 1;
}

FTP_FUNC_DEFINE(CWD) {
    if (client->ftp_state == LOGGED_IN) {
        if (argument) {
            const char *fullpath = NULL;
            int free_flag = 0;
            if (!strcmp(argument, "/")) {
                fullpath = service_root;
            } else if (argument[0] == '/') {
                fullpath = fs_path_join(service_root, argument);
                free_flag = 1;
            } else if (argument[0] == '.' && argument[1] == '.' && argument[2] == 0) {
                /* 上一级目录 */
                fullpath = fs_path_backward(client->cwd);
                free_flag = 1;
            } else {
                fullpath = fs_path_join(client->cwd, argument);
                free_flag = 1;
            }
            if (fs_directory_exists(fullpath)) {
                if (fs_directory_allows(service_root, fullpath)) {
                    strcpy(client->cwd, fullpath);
                    protocol_client_resp_by_state_machine(client, 250, "Okay.");
                } else {
                    protocol_client_resp_by_state_machine(client, 550, "Not in the root folder.");
                }
            } else {
                protocol_client_resp_by_state_machine(client, 550, "No such file or directory.");
            }

            if (free_flag) {
                free((void *) fullpath);
            }
            return 0;
        }
    }
    return 1;
}

FTP_FUNC_DEFINE(CDUP) {
    if (client->ftp_state == LOGGED_IN) {
        if (!argument) {
            const char *fullpath = NULL;
            fullpath = fs_path_backward(client->cwd);
            printf("%s\r\n", fullpath);
            if (fs_directory_exists(fullpath)) {
                if (fs_directory_allows(service_root, fullpath)) {
                    strcpy(client->cwd, fullpath);
                    protocol_client_resp_by_state_machine(client, 250, "Okay.");
                } else {
                    protocol_client_resp_by_state_machine(client, 550, "Not in the root folder.");
                }
            } else {
                protocol_client_resp_by_state_machine(client, 550, "No such file or directory.");
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
                protocol_client_resp_by_state_machine(client, 200, "Type set to Ascii.");
                return 0;
            } else if (!strcmp(argument, "I")) {
                client->data_type = BINARY;
                protocol_client_resp_by_state_machine(client, 200, "Type set to I.");
                return 0;
            } else if (!strcmp(argument, "L")) {
                client->data_type = BINARY;
                protocol_client_resp_by_state_machine(client, 200, "Type set to L.");
                return 0;
            }
        }
    }

    return 1;
}

FTP_FUNC_DEFINE(SYST) {
    if (client->ftp_state == LOGGED_IN) {
        if (!argument) {
            protocol_client_resp_by_state_machine(client, 215, "UNIX Type: L8");
            return 0;
        }
    }

    return 1;
}

FTP_FUNC_DEFINE(QUIT) {
    if (client->ftp_state == LOGGED_IN) {
        if (!argument) {
            protocol_client_resp_by_state_machine(client, 221, "Goodbye");
            if (client->conn_type == PASV) {
                pasv_close_connection(client->pasv_port);
            } else if (client->conn_type == PORT) {
                port_close_connection(client);
            }
            protocol_client_quit(client);
            return 0;
        }
    }

    return 1;
}

FTP_FUNC_DEFINE(PASV) {
    if (client->ftp_state == LOGGED_IN) {
        if (!argument) {
            if (!pasv_client_new(&client->pasv_port)) {
                client->conn_type = PASV;
                char *resp = malloc(128);
                sprintf(resp, "Entering Passive Mode (%d,%d,%d,%d,%d,%d)",
                        (load_ip_addr >> 24) & 0xff,
                        (load_ip_addr >> 16) & 0xff,
                        (load_ip_addr >> 8) & 0xff,
                        load_ip_addr & 0xff,
                        client->pasv_port / 256,
                        client->pasv_port % 256);
                protocol_client_resp_by_state_machine(client, 227, resp);
                free(resp);
            } else {
                client->conn_type = NOT_SPECIFIED;
                protocol_client_resp_by_state_machine(client, 434, "No available port assigned for this PASV.");
            }
            return 0;
        }
    }

    return 1;
}

FTP_FUNC_DEFINE(PORT) {
    if (client->ftp_state == LOGGED_IN) {
        if (argument) {
            int cnt = 0;
            int digits[6];
            char address_port[128];
            strcpy(address_port, argument);
            char *segment, *endptr;
            for (segment = strtok(address_port, ","); segment; segment = strtok(NULL, ",")) {
                digits[cnt++] = (int) strtol(segment, &endptr, 10);
                if (*endptr != '\0') {
                    goto fail;
                }
            }
            if (cnt != 6) {
                goto fail;
            }
            uint32_t target_ip =
                    (digits[0] << 24) |
                    (digits[1] << 16) |
                    (digits[2] << 8) |
                    digits[3];

            client->port_target_addr.sin_family = AF_INET;
            client->port_target_addr.sin_port = htons(digits[4] * 256 + digits[5]);
            client->port_target_addr.sin_addr.s_addr = htonl(target_ip);

            if (!port_client_new(client->port_target_addr)) {
                client->conn_type = PORT;
                protocol_client_resp_by_state_machine(client, 200, "PORT command successful.");
            } else {
                client->conn_type = NOT_SPECIFIED;
                protocol_client_resp_by_state_machine(client, 434, "Failed to start PORT.");
            }
            return 0;
        }
    }

    fail:
    return 1;
}

FTP_FUNC_DEFINE(REST) {
    if (client->ftp_state == LOGGED_IN) {
        if (argument) {
            long offset;
            char *endptr;
            offset = strtol(argument, &endptr, 10);
            if (*endptr != '\0') {
                goto fail;
            }

            if (client->conn_type == PASV) {
                pasv_send_set_rest(client->pasv_port, offset);
                protocol_client_resp_by_state_machine(client, 350, "PASV REST successful.");
            } else if (client->conn_type == PORT) {
                port_send_set_rest(client, offset);
                protocol_client_resp_by_state_machine(client, 350, "PORT REST successful.");
            } else {
                client->conn_type = NOT_SPECIFIED;
                protocol_client_resp_by_state_machine(client, 434, "You must specify PASV or PORT first.");
            }
            return 0;
        }
    }

    fail:
    return 1;
}

FTP_FUNC_DEFINE(LIST) {
    if (client->ftp_state == LOGGED_IN) {
        if (!argument) {
            if (client->conn_type == NOT_SPECIFIED) {
                protocol_client_resp_by_state_machine(client, 550, "Specify the PORT/PASV mode.");
                return 0;
            } else {
                if (!fs_directory_allows(service_root, client->cwd)) {
                    protocol_client_resp_by_state_machine(client, 550, "Not in the root folder.");
                    return 0;
                }
                char shell[384];
                memset(shell, 0, 256);
                sprintf(shell, "ls -l %s", client->cwd);
                FILE *fp = popen(shell, "r");
                if (!fp) {
                    protocol_client_resp_by_state_machine(client, 451, "Failed to call ls -l.");
                    return 0;
                } else {
                    const size_t init_len = 8192;
                    size_t buf_len = init_len;
                    char *buffer = malloc(init_len);
                    char *formatted = malloc(init_len);
                    size_t len = 0;
                    memset(buffer, 0, init_len);
                    memset(formatted, 0, init_len);
                    while (fgets(buffer + len, buf_len - 10, fp) != NULL) {
                        len = strlen(buffer);
                        if (len >= buf_len - 1024) {
                            buf_len *= 2;
                            char *new_buffer = malloc(buf_len);
                            char *new_fmt = malloc(buf_len);

                            strcpy(new_buffer, buffer);
                            free(buffer);
                            free(formatted);
                            buffer = new_buffer;
                            formatted = new_fmt;
                        }
                    }
                    pclose(fp);
                    char *line;
                    size_t fmt_len = 0;
                    for (line = strtok(buffer, "\r\n"); line; line = strtok(NULL, "\r\n")) {
                        char tmp = line[5];
                        line[5] = 0;
                        if (!strcmp(line, "total")) {
                            line[5] = tmp;
                            continue;
                        }
                        line[5] = tmp;
                        strcpy(formatted + fmt_len, line);
                        fmt_len = strlen(formatted);
                        formatted[fmt_len] = '\r';
                        formatted[fmt_len + 1] = '\n';
                        fmt_len += 2;
                    }
                    free(buffer);
                    /* buffer 由 pasv 进行释放 */
                    protocol_client_resp_by_state_machine(client, 150, "Here comes the directory listing.");
                    char tmp[64];
                    sprintf(tmp, "%d %s\r\n", 226, "Directory send OK.");
                    if (client->conn_type == PASV) {
                        pasv_send_data(client->pasv_port, formatted, fmt_len, client, tmp);
                    } else {
                        port_send_data(client, formatted, fmt_len, tmp);
                    }
                }
            }
            return 0;
        }
    }

    return 1;
}

FTP_FUNC_DEFINE(MKD) {
    if (client->ftp_state == LOGGED_IN) {
        if (argument) {
            const char *fullpath = NULL;
            int free_flag = 0;
            if (!strcmp(argument, "/")) {
                fullpath = service_root;
            } else if (argument[0] == '/') {
                fullpath = fs_path_join(service_root, argument);
                free_flag = 1;
            } else if (argument[0] == '.' && argument[1] == '.' && argument[2] == 0) {
                /* 上一级目录 */
                fullpath = fs_path_backward(client->cwd);
                free_flag = 1;
            } else {
                fullpath = fs_path_join(client->cwd, argument);
                free_flag = 1;
            }
            if (fs_directory_allows(service_root, fullpath)) {
                char *cmd = malloc(256);
                sprintf(cmd, "mkdir -p %s", fullpath);
                int ret = system(cmd);
                free(cmd);
                if (!ret) {
                    char *resp = malloc(300);
                    char *free_handle;
                    const char *rel_path = fs_path_erase(service_root, fullpath, &free_handle);
                    sprintf(resp, "\"%s\" has been created", rel_path);
                    protocol_client_resp_by_state_machine(client, 257, resp);
                    free(free_handle);
                    free(resp);
                } else {
                    protocol_client_resp_by_state_machine(client, 550, "Failed to create the directory.");
                }
            } else {
                protocol_client_resp_by_state_machine(client, 550, "Not in the root folder.");
            }

            if (free_flag) {
                free((void *) fullpath);
            }
            return 0;
        }
    }
    return 1;
}

FTP_FUNC_DEFINE(RMD) {
    if (client->ftp_state == LOGGED_IN) {
        if (argument) {
            const char *fullpath = NULL;
            int free_flag = 0;
            if (!strcmp(argument, "/")) {
                fullpath = service_root;
            } else if (argument[0] == '/') {
                fullpath = fs_path_join(service_root, argument);
                free_flag = 1;
            } else if (argument[0] == '.' && argument[1] == '.' && argument[2] == 0) {
                /* 上一级目录 */
                fullpath = fs_path_backward(client->cwd);
                free_flag = 1;
            } else {
                fullpath = fs_path_join(client->cwd, argument);
                free_flag = 1;
            }
            if (fs_directory_exists(fullpath)) {
                if (fs_directory_allows(service_root, fullpath)) {
                    char *cmd = malloc(256);
                    sprintf(cmd, "rm -rf %s", fullpath);
                    int ret = system(cmd);
                    free(cmd);
                    if (!ret) {
                        protocol_client_resp_by_state_machine(client, 250, "Directory was removed successfully.");
                    } else {
                        protocol_client_resp_by_state_machine(client, 550, "Failed to remove the directory.");
                    }
                } else {
                    protocol_client_resp_by_state_machine(client, 550, "Not in the root folder.");
                }
            } else {
                protocol_client_resp_by_state_machine(client, 550, "No such file or directory.");
            }

            if (free_flag) {
                free((void *) fullpath);
            }
            return 0;
        }
    }
    return 1;
}

FTP_FUNC_DEFINE(RETR) {
    if (client->ftp_state == LOGGED_IN) {
        if (client->conn_type == NOT_SPECIFIED) {
            protocol_client_resp_by_state_machine(client, 550, "Specify the PORT/PASV mode.");
            return 0;
        } else {
            if (argument) {
                /* 这里的argument仅仅判断是不是有文件名作为参数存在，真正的文件名
                 * 参数应该从full_instruction中获取
                 */
                char tmp = client->full_instruction[5];
                client->full_instruction[5] = 0;
                if ((strcmp(client->full_instruction, "RETR ") != 0) &&
                    (strcmp(client->full_instruction, "retr ") != 0)) {
                    return 1;
                }
                client->full_instruction[5] = tmp;
                argument = &client->full_instruction[5];
                const char *fullpath = NULL;
                if (strstr(argument, "../") != NULL) {
                    /* 含有 ../，禁止执行 */
                    protocol_client_resp_by_state_machine(client, 550, "Must not contain '../'.");
                    return 0;
                }
                if (argument[0] == '/') {
                    fullpath = fs_path_join(service_root, argument);
                } else {
                    fullpath = fs_path_join(client->cwd, argument);
                }
                if (fs_directory_allows(service_root, fullpath)) {
                    if (fs_file_exists(fullpath)) {
                        const char *ascii = "ASCII";
                        const char *binary = "BINARY";
                        char *resp = malloc(512);
                        sprintf(resp, "Opening %s mode data connection for %s (%zu bytes).",
                                client->data_type == BINARY ? binary : ascii,
                                fs_get_filename(fullpath),
                                fs_get_file_size(fullpath));
                        protocol_client_resp_by_state_machine(client, 150, resp);
                        sprintf(resp, "%d %s\r\n", 226, "Transfer complete.");
                        if (client->conn_type == PASV) {
                            pasv_sendfile(client->pasv_port, fullpath, client, resp);
                        } else {
                            port_sendfile(client, fullpath, resp);
                        }
                        free(resp);
                    } else {
                        protocol_client_resp_by_state_machine(client, 550, "No such file or directory.");
                    }
                } else {
                    protocol_client_resp_by_state_machine(client, 550, "Not in the root folder.");
                }
                free((void *) fullpath);
                return 0;
            }
        }
    }
    return 1;
}

FTP_FUNC_DEFINE(STOR) {
    if (client->ftp_state == LOGGED_IN) {
        if (client->conn_type == NOT_SPECIFIED) {
            protocol_client_resp_by_state_machine(client, 550, "Specify the PORT/PASV mode.");
            return 0;
        } else {
            if (argument) {
                /* 这里的argument仅仅判断是不是有文件名作为参数存在，真正的文件名
                 * 参数应该从full_instruction中获取
                 */
                char tmp = client->full_instruction[5];
                client->full_instruction[5] = 0;
                if ((strcmp(client->full_instruction, "STOR ") != 0) &&
                    (strcmp(client->full_instruction, "stor ") != 0)) {
                    return 1;
                }
                client->full_instruction[5] = tmp;
                argument = &client->full_instruction[5];
                const char *fullpath = NULL;
                if (strstr(argument, "../") != NULL) {
                    /* 含有 ../，禁止执行 */
                    protocol_client_resp_by_state_machine(client, 550, "Must not contain '../'.");
                    return 0;
                }
                if (argument[0] == '/') {
                    fullpath = fs_path_join(service_root, argument);
                } else {
                    fullpath = fs_path_join(client->cwd, argument);
                }
                if (fs_directory_allows(service_root, fullpath)) {
                    char *dir = malloc(256);
                    fs_get_directory(fullpath, dir);
                    if (!fs_directory_exists(dir)) {
                        char *cmd = malloc(256);
                        sprintf(cmd, "mkdir -p %s", dir);
                        int ret = system(cmd);
                        free(cmd);
                        if (ret != 0) {
                            protocol_client_resp_by_state_machine(client, 451, "Cannot create the folder.");
                            return 0;
                        }
                    }
                    free(dir);
                    const char *ascii = "ASCII";
                    const char *binary = "BINARY";
                    char *resp = malloc(512);
                    sprintf(resp, "%d %s\r\n", 226, "Transfer complete.");
                    if (client->conn_type == PASV) {
                        pasv_recvfile(client->pasv_port, fullpath, client, resp);
                    } else {
                        port_recvfile(client, fullpath, resp);
                    }
                    sprintf(resp, "Opening %s mode data connection for %s.",
                            client->data_type == BINARY ? binary : ascii,
                            fs_get_filename(fullpath));
                    protocol_client_resp_by_state_machine(client, 150, resp);
                    free(resp);
                } else {
                    protocol_client_resp_by_state_machine(client, 550, "Not in the root folder.");
                }
                free((void *) fullpath);
                return 0;
            }
        }
    }
    return 1;
}

FTP_FUNC_DEFINE(APPE) {
    return FTP_STOR(client, argument);
}
