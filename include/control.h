#pragma once
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "general.h"

#include "crypto.h"
#include "filesystem.h"
#include "site.h"
#include "log.h"
#include "parse.h"
#include "net.h"
#include "config.h"
#include "transfer_result.h"
#include "stats.h"
#include "transfer_result.h"
#include "command.h"

#define CONTROL_BUF_SZ 1024 // max number of bytes we can get at once
#define CONTROL_LINE_SZ 1024
#define CONTROL_INT_BUF_CHUNK_SZ 1024

struct pasv_details {
	char host[255];
	uint32_t port;
	char unparsed[255];
};

bool control_send(struct site_info *site, char *data);
int32_t control_recv(struct site_info *site);
