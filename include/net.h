#pragma once

#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>

#include "general.h"
#include "log.h"

#define CONN_BACKLOG 20

void *net_get_in_addr(struct sockaddr *sa);
int32_t net_open_server_socket(char *port);
int32_t net_open_socket(char *address, char *port);
