#pragma once

#include "general.h"
#include "site.h"
#include "log.h"
#include "ftp.h"

struct transfer_result *fxp(struct site_info *src, struct site_info *dst, const char *filename, const char *src_dir, const char *dst_dir);
struct transfer_result *fxp_recursive(struct site_info *src, struct site_info *dst, const char *dirname, const char *src_dir, const char *dst_dir);
