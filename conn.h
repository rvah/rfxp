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

#define CONTROL_BUF_SZ 1024 // max number of bytes we can get at once 
#define CONTROL_LINE_SZ 1024
#define CONTROL_INT_BUF_CHUNK_SZ 1024

#define DATA_BUF_SZ 1024

struct pasv_details {
	char host[255];
	uint32_t port;
};

bool control_send(struct site_info *site, char *data);
int32_t control_recv(struct site_info *site);
bool ftp_connect(struct site_info *site);
void ftp_disconnect(struct site_info *site);
bool ftp_retr(struct site_info *site, char *filename);
bool ftp_get(struct site_info *site, char *filename, char *local_dir);
bool ftp_ls(struct site_info *site);
bool ftp_get_recursive(struct site_info *site, char *dirname, char *local_dir);
