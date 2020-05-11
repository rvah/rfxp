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

#define CONTROL_BUF_SZ 1024 // max number of bytes we can get at once 
#define CONTROL_LINE_SZ 1024
#define CONTROL_INT_BUF_CHUNK_SZ 1024

#define DATA_BUF_SZ 4096

struct pasv_details {
	char host[255];
	uint32_t port;
	char unparsed[255];
};

bool control_send(struct site_info *site, char *data);
int32_t control_recv(struct site_info *site);
bool ftp_connect(struct site_info *site);
void ftp_disconnect(struct site_info *site);
bool ftp_retr(struct site_info *site, char *filename);
struct transfer_result *ftp_get(struct site_info *site, char *filename, char *local_dir, char *remote_dir);
struct transfer_result *ftp_put(struct site_info *site, char *filename, char *local_dir, char *remote_dir);
bool ftp_cwd(struct site_info *site, const char *dirname);
bool ftp_mkd(struct site_info *site, char *dirname);
bool ftp_ls(struct site_info *site);
struct transfer_result *ftp_get_recursive(struct site_info *site, char *dirname, char *local_dir, char *remote_dir);
struct transfer_result *ftp_put_recursive(struct site_info *site, char *dirname, char *local_dir, char *remote_dir);
struct transfer_result *fxp(struct site_info *src, struct site_info *dst, char *filename, char *src_dir, char *dst_dir);
struct transfer_result *fxp_recursive(struct site_info *src, struct site_info *dst, char *dirname, char *src_dir, char *dst_dir);
