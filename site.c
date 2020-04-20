#include "site.h"

struct site_pair *current_pair = NULL;

void site_set_cwd(struct site_info *site, char *cwd) {
	strlcpy(site->current_working_dir, cwd, MAX_PATH_LEN);
}



struct site_info *site_init(char *name, char *address, char *port, char *username, char *password, bool use_tls) {
	struct site_info *site = malloc(sizeof(struct site_info));
	
	strlcpy(site->name, name, SITE_NAME_MAX);
	strlcpy(site->address, address, SITE_HOST_MAX);
	strlcpy(site->port, port, SITE_PORT_MAX);
	strlcpy(site->username, username, SITE_USER_MAX);
	strlcpy(site->password, password, SITE_PASS_MAX);
	site->use_tls = use_tls;
	site->last_recv = NULL;

	site_set_cwd(site, "/");

	return site;
}

struct site_pair *site_get_current_pair() {
	if(current_pair == NULL) {
		current_pair = malloc(sizeof(struct site_pair));
		current_pair->left = NULL;
		current_pair->right = NULL;
		current_pair->id = site_gen_id();
	}

	return current_pair;
}

void site_destroy_pair(struct site_pair *pair) {
	free(pair->left);
	free(pair->right);
	free(pair);
}

uint32_t site_gen_id() {
	static uint32_t id = 0;
	return id++;
}
