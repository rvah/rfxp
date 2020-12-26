#pragma once
#include <pthread.h>
#include "general.h"
#include <readline/readline.h>
#include "site.h"
#include "control.h"
#include "msg.h"
#include "log.h"
#include "colors.h"
#include "filesystem.h"
#include "util.h"
#include "ui_helpers.h"
#include "ui_indicator.h"
#include "ident.h"
#include "command.h"
#include "queue.h"
#include "ftp.h"
#include "fxp.h"

struct fxp_arg {
	char *filename;
	struct site_info *dst;
};

void *thread_ident(void *ptr);
void *thread_indicator(void *ptr);
void *thread_ui(void *ptr);
void *thread_site(void *ptr);
void *thread_run_queue(void *ptr);
