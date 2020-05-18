#include "site.h"

struct site_pair *current_pair = NULL;

void site_busy(struct site_info *site) {
	site->busy = true;
}

void site_idle(struct site_info *site) {
	site->busy = false;
}

void site_set_cwd(struct site_info *site, char *cwd) {
	strlcpy(site->current_working_dir, cwd, MAX_PATH_LEN);
}

void site_list_add_site(struct site_list **list, struct site_info *site) {
	if(*list == NULL) {
		(*list) = malloc(sizeof(struct site_list));
		(*list)->site = site;
		(*list)->next = NULL;
	} else {
		struct site_list *new_node = malloc(sizeof(struct site_list));
		new_node->site = site;
		new_node->next = *list;
		*list = new_node;
	}	
}

struct site_list *site_get_all() {
	struct site_pair *p = site_get_current_pair();

	struct site_list *list = NULL;

	if(p->left != NULL) {
		site_list_add_site(&list, p->left);
	}

	if(p->right != NULL) {
		site_list_add_site(&list, p->right);
	}

	return list;
}

void site_destroy_list(struct site_list *list) {
	struct site_list *l = NULL;
	while(list != NULL) {
		l = list;
		list = list->next;
		free(l);
	}
}

struct site_list *site_get_sites_connecting() {
	struct site_list *all_sites = site_get_all();
	struct site_list *l = all_sites;
	struct site_list *r = NULL;

	while(l != NULL) {
		if(l->site->is_connecting) {
			site_list_add_site(&r, l->site);
		}

		l = l->next;		
	}	

	return r;
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
	site->prot_sent = false;
	site->busy = false;
	site->current_speed = 0.0f;
	site->ls_do_cache = false;
	site->enable_sscn = false;
	site->sscn_on = false;
	site->enforce_sscn_server_mode = false;
	site->is_connecting = false;
	site->xdupe_enabled = false;
	site->xdupe_table = dict_create();
	site->xdupe_empty = true;
	site->cur_dirlist = NULL;

	site_set_cwd(site, "/");


	return site;
}

void site_xdupe_add(struct site_info *site, const char *file) {
	if(!dict_has_key(site->xdupe_table, file)) {
		dict_set(site->xdupe_table, file, NULL);
		site->xdupe_empty = false;
	}
}

void site_xdupe_clear(struct site_info *site) {
	dict_clear(site->xdupe_table, false);
	site->xdupe_empty = true;
}

bool site_xdupe_has(struct site_info *site, const char *file) {
	return dict_has_key(site->xdupe_table, file);
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
