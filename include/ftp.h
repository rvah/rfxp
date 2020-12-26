#pragma once

#include "general.h"
#include "site.h"
#include "control.h"
#include "data.h"

#define FTP_DATA_BUF_SZ 4096

bool ftp_connect(struct site_info *site);
void ftp_disconnect(struct site_info *site);
bool ftp_retr(struct site_info *site, char *filename);
bool ftp_cwd(struct site_info *site, const char *dirname);
bool ftp_mkd(struct site_info *site, char *dirname);
bool ftp_ls(struct site_info *site);
bool ftp_sscn(struct site_info *site, bool set_on);
bool ftp_prot(struct site_info *site);
bool ftp_pwd(struct site_info *site);
bool ftp_feat(struct site_info *site);
bool ftp_xdupe(struct site_info *site, uint32_t level);
bool ftp_auth(struct site_info *site);
bool ftp_stor(struct site_info *site, char *filename);
bool ftp_cwd_up(struct site_info *site);
struct pasv_details *ftp_pasv(struct site_info *site, bool handshake);
struct transfer_result *ftp_get(struct site_info *site, const char *filename, const char *local_dir, const char *remote_dir);
struct transfer_result *ftp_put(struct site_info *site, const char *filename, const char *local_dir, const char *remote_dir);
struct transfer_result *ftp_get_recursive(struct site_info *site, const char *dirname, const char *local_dir, const char *remote_dir);
struct transfer_result *ftp_put_recursive(struct site_info *site, const char *dirname, const char *local_dir, const char *remote_dir);
