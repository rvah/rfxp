#pragma once

#include <openssl/ssl.h>
#include <pthread.h>
#include "general.h"
#include "filesystem.h"

#define SITE_NAME_MAX 1024
#define SITE_HOST_MAX 1024
#define SITE_USER_MAX 1024
#define SITE_PASS_MAX 1024
#define SITE_PORT_MAX 6

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
	char current_working_dir[MAX_PATH_LEN];
	struct linked_str_node *cmd_list;
	pthread_t thread;
	uint32_t thread_id;
	char *last_recv;
	bool prot_sent;
	struct file_item *cur_dirlist;
};

struct site_pair {
	uint32_t id;
	struct site_info *left;
	struct site_info *right;
};

void site_set_cwd(struct site_info *site, char *cwd);
struct site_info *site_init(char *name, char *address, char *port, char *username, char *password, bool use_tls);
struct site_pair *site_get_current_pair();
void site_destroy_pair(struct site_pair *pair);
uint32_t site_gen_id();
