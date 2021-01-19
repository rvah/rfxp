#include "sm.h"

bool sm_add_site(const char *name, const char *username, const char *password,
		const char *host, const char *port) {
	if(get_site_config_by_name(name) != NULL) {
		log_ui_e("site with name '%s' already exists.\n", name);
		return false;
	}

	struct site_config *new_site = malloc(sizeof(struct site_config));

	new_site->id = 0;

	//assign new id
	struct site_config *all = config_get_conf()->sites;

	while(all != NULL) {
		if(all->id >= new_site->id) {
			new_site->id = all->id+1;
		}
		all = all->next;
	}

	strlcpy(new_site->name, name, 255);
	strlcpy(new_site->pass, password, 255);
	strlcpy(new_site->user, username, 255);
	strlcpy(new_site->host, host, 255);
	strlcpy(new_site->port, port, 6);

	new_site->tls = true;
	new_site->next = NULL;

	add_site_config(new_site);

	log_ui_i("added site '%s'\n", new_site->name);

	//commit changes to disk
	if(!write_site_config_file(config_get_conf()->sites, "noob")) {
		log_ui_e("Error writing sitedb to disk!\n");
		return false;
	}

	return true;
}

bool sm_remove_site(const char *name) {
	if((name == NULL) || (get_site_config_by_name(name) == NULL)) {
		log_ui_e("could not find any site called '%s'\n", name);
		return false;
	}

	struct site_config *all = config_get_conf()->sites;
	struct site_config *prev = NULL;

	while(all != NULL) {
		if(strcasecmp(all->name, name) == 0) {
			if(prev == NULL) {
				config_get_conf()->sites = all->next;
			} else {
				prev->next = all->next;
			}

			log_ui_i("deleted site '%s'\n", all->name);

			free(all);
			break;
		}

		prev = all;
		all = all->next;
	}

	//commit changes to disk
	if(!write_site_config_file(config_get_conf()->sites, "noob")) {
		log_ui_e("Error writing sitedb to disk!\n");
		return false;
	}

	return true;
}

void sm_list_all() {
	struct site_config *s = config_get_conf()->sites;
	if(s == NULL) {
		log_ui_i("no sites added.\n");
		return;
	}

	log_ui_i("Added sites:\n");

	while(s != NULL) {
		printf("%s", s->name);

		s = s->next;

		if(s != NULL) {
			printf(", ");
		}
	}

	printf("\n");
}

void sm_list(const char *name) {
	struct site_config *site = get_site_config_by_name(name);

	if(site == NULL) {
		log_ui_e("could not find any site called '%s'\n", name);
	} else {
		log_ui_i("ID: %d\n", site->id);
		log_ui_i("name: %s\n", site->name);
		log_ui_i("host: %s\n", site->host);
		log_ui_i("port: %s\n", site->port);
		log_ui_i("user: %s\n", site->user);
		log_ui_i("pass: <hidden>\n");
	}
}

void sm_edit(const char *name, const char *setting, const char *value) {
	struct site_config *site = get_site_config_by_name(name);

	if(site == NULL) {
		log_ui_e("could not find any site called '%s'\n", name);
		return;
	}

	if(strcmp(setting, "name") == 0) {
		if(strlen(value) == 0) {
			log_ui_e("bad name format\n");
			return;
		}

		strlcpy(site->name, value, 255);
	} else if(strcmp(setting, "host") == 0) {
		if(strstr(value, ":") == NULL) {
			log_ui_e("bad host:port format, please fix and try again.\n");
			return;
		}

		char *save;
		char *hport = strdup(value);
		char *host = strtok_r(hport, ":", &save);
		char *port = strtok_r(NULL, ":", &save);

		strlcpy(site->host, host, 255);
		strlcpy(site->port, port, 6);

		free(hport);
	} else if(strcmp(setting, "user") == 0) {
		if(strlen(value) == 0) {
			log_ui_e("bad username format\n");
			return;
		}

		strlcpy(site->user, value, 255);
	} else if(strcmp(setting, "pass") == 0) {
		if(strlen(value) == 0) {
			log_ui_e("bad password format\n");
			return;
		}

		strlcpy(site->pass, value, 255);
	} else {
		log_ui_e("%s: unknown attribute\n", setting);
		return;
	}

	//commit changes
	if(!write_site_config_file(config_get_conf()->sites, "noob")) {
		log_ui_e("Error writing sitedb to disk!\n");
		return;
	}
}
