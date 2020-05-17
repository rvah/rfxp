#pragma once
#include "general.h"
#include "libs/inih/ini.h"
#include "skiplist.h"
#include "priolist.h"
#include "hilight.h"
#include "ident.h"
#include "colors.h"
#include "crypto.h"

#define MFXP_CONF_DIR "~/.mfxp/"
#define SITE_CONFIG_FILE_PATH "~/.mfxp/sitedb.dat"

struct sites_file_head {
	char magic[5];
	uint32_t n_sites;
	uint32_t reserved1;
	uint32_t reserved2;
	uint32_t reserved3;
	uint32_t reserved4;
};

struct site_config {
	uint32_t id;
	char name[255];
	char host[255];
	char port[6];
	char user[255];
	char pass[255];
	bool tls;
	struct site_config *next;
};

struct config {
	struct site_config *sites;
	bool enable_xdupe;
	bool show_dirlist_on_cwd;
};

struct config *config_get_conf();

void config_cleanup();
struct site_config *get_site_config_by_name(char *name);
void add_site_config(struct site_config *conf);
bool config_read(char *path);

bool write_site_config_file(struct site_config *sites, const char *key);
struct site_config *read_site_config_file(const char *key);
