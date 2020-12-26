#pragma once
#include <linux/limits.h>
#include "general.h"
#include "site.h"
#include "control.h"
#include "ftp.h"
#include "fxp.h"

#define Q_ITEM_TYPE_PUT 0
#define Q_ITEM_TYPE_GET 1
#define Q_ITEM_TYPE_FXP 2

#define Q_ITEM_STAT_PENDING 0
#define Q_ITEM_STAT_RUNNING 1
#define Q_ITEM_STAT_SUCCESS 2
#define Q_ITEM_STAT_FAILURE 3

struct queue_item {
	int id;
	struct queue_item *next;
	uint32_t type;
	uint32_t status;
	struct site_info *site_src;
	struct site_info *site_dst;
	char *path_src;
	char *path_dst;
	char *filename;
	bool is_dir;
};

void queue_remove(uint32_t id);
void queue_print();
bool queue_running();
void queue_execute();
void queue_add_put(struct site_info *site, char *path_local, char *path_site);
void queue_add_get(struct site_info *site, char *path_site, char *path_local);
void queue_add_fxp(struct site_info *site_src, struct site_info *site_dst, char *path_src, char *path_dst);
