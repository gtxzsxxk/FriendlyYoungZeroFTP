#ifndef SERVER_H_LISTENER
#define SERVER_H_LISTENER
#include <netinet/in.h>

extern struct sockaddr_in local_addr;
extern uint32_t load_ip_addr;
extern socklen_t local_len;

int start_listen(int port);

#endif
