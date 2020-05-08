#pragma once
#include <pthread.h>
#include <readline/readline.h>
#include "general.h"
#include "site.h"
#include "conn.h"
#include "msg.h"
#include "log.h"
#include "colors.h"
#include "filesystem.h"
#include "util.h"
#include "ui_helpers.h"
#include "ui_indicator.h"

struct fxp_arg {
	char *filename;
	struct site_info *dst;
};

void *thread_indicator(void *ptr);
void *thread_ui(void *ptr);
void *thread_site(void *ptr);
