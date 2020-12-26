#pragma once

#include <openssl/ssl.h>
#include <pthread.h>
#include "general.h"
#include "filesystem.h"
#include "dictionary.h"

#define SITE_NAME_MAX 1024
#define SITE_HOST_MAX 1024
#define SITE_USER_MAX 1024
#define SITE_PASS_MAX 1024
#define SITE_PORT_MAX 6

struct site_list {
	struct site_list *next;
	struct site_info *site;
};

struct site_info {
	char name[SITE_NAME_MAX];
	char address[SITE_HOST_MAX];
	char port[SITE_PORT_MAX];
	char username[SITE_USER_MAX];
	char password[SITE_PASS_MAX];
	int32_t socket_fd;
	int32_t data_socket_fd;
	bool use_tls;
	SSL* secure_fd;
	SSL* data_secure_fd;
	bool is_secure;
	char current_working_dir[MAX_PATH_LEN];
	struct linked_str_node *cmd_list;
	pthread_t thread;
	uint32_t thread_id;
	char *last_recv;
	bool prot_sent;
	struct file_item *cur_dirlist;
	bool busy;
	double current_speed;
	bool ls_do_cache;
	bool enable_sscn;
	bool sscn_on;
	bool enforce_sscn_server_mode;
	bool is_connecting;
	bool xdupe_enabled;
	struct dict_node **xdupe_table;
	bool xdupe_empty;
};

struct site_pair {
	uint32_t id;
	struct site_info *left;
	struct site_info *right;
};

struct site_info *site_init(char *name, char *address, char *port, char *username, char *password, bool use_tls);
void site_busy(struct site_info *site);
void site_idle(struct site_info *site);
void site_set_cwd(struct site_info *site, char *cwd);
struct site_list *site_get_all();
void site_destroy_list(struct site_list *list);
struct site_list *site_get_sites_connecting();
void site_xdupe_add(struct site_info *site, const char *file);
void site_xdupe_clear(struct site_info *site);
bool site_xdupe_has(struct site_info *site, const char *file);
struct site_pair *site_get_current_pair();
void site_destroy_pair(struct site_pair *pair);
uint32_t site_gen_id();
