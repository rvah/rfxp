#include "config.h"

static struct config *app_conf = NULL;

/*
 * ----------------
 *
 * Private functions
 *
 * ----------------
 */

static bool init() {
	app_conf = malloc(sizeof(struct config));
	app_conf->sites = NULL;
	app_conf->enable_xdupe = false;

	char *path = expand_home_path(MFXP_CONF_DIR);

	//check if config dir exist
	if(!file_exists(path)) {
		if(mkdir(path, 0755) != 0) {
			printf("failed creating dir: %s\n", path);
			free(path);
			return false;
		}
	}

	free(path);

	return true;
}

/*
static void print_site_configs() {
	if(app_conf->sites == NULL) {
		printf("no sites added!\n");
		return;
	}

	struct site_config *p = app_conf->sites;

	while(p != NULL) {
		printf("--- %s ---\n", p->name);
		printf("name: %s\n", p->name);
		printf("host: %s\n", p->host);
		printf("port: %s\n", p->port);
		printf("user: %s\n", p->user);
		printf("pass: %s\n", p->pass);
		p = p->next;
	}
}
*/

static int ini_read_handler(void* user, const char* section, const char* name, const char* value) {
	char *save;
	char *s_name = strtok_r(strdup(section), "_", &save);

	if(strcmp(s_name, "general") == 0) {
		if(strcmp(name, "skiplist") == 0) {
			if(!skiplist_init(value)) {
				printf("failed to init skiplist.\n");
				return 0;
			}
		} else if(strcmp(name, "priolist") == 0) {
			if(!priolist_init(value)) {
				printf("failed to init priolist.\n");
				return 0;
			}
		} else if(strcmp(name, "hilight") == 0) {
			if(!hilight_init(value)) {
				printf("failed to init hilight.\n");
				return 0;
			}
		} else if(strcmp(name, "enable_xdupe") == 0) {
			char *s_xdupe = strdup(value);
			str_trim(s_xdupe);
			app_conf->enable_xdupe = strcmp(s_xdupe, "true") == 0;
			free(s_xdupe);
		} else if(strcmp(name, "default_local_dir") == 0) {
			char *d_val = strdup(value);
			str_trim(d_val);

			//expand ~
			if(d_val[0] == '~') {
				wordexp_t exp_result;
				wordexp(d_val, &exp_result, 0);

				free(d_val);
				d_val = strdup(exp_result.we_wordv[0]);
			}

			if(chdir(d_val) != 0) {
				free(d_val);
				printf("failed to set default dir: %s\n", value);
				return 0;
			}

			free(d_val);
		} else if(strcmp(name, "show_dirlist_on_cwd") == 0) {
			char *s_show = strdup(value);
			str_trim(s_show);
			app_conf->show_dirlist_on_cwd = strcmp(value, "true") == 0;
			free(s_show);
			return 0;
		} else if(strcmp(name, "default_sort") == 0) {
			char *s_sort = strdup(value);
			str_trim(s_sort);

			if(strcmp(s_sort, "time_asc") == 0) {
				filesystem_set_sort(SORT_TYPE_TIME_ASC);
			} else if(strcmp(s_sort, "time_desc") == 0) {
				filesystem_set_sort(SORT_TYPE_TIME_DESC);
			} else if(strcmp(s_sort, "name_asc") == 0) {
				filesystem_set_sort(SORT_TYPE_NAME_ASC);
			} else if(strcmp(s_sort, "name_desc") == 0) {
				filesystem_set_sort(SORT_TYPE_NAME_DESC);
			} else if(strcmp(s_sort, "size_asc") == 0) {
				filesystem_set_sort(SORT_TYPE_SIZE_ASC);
			} else if(strcmp(s_sort, "size_desc") == 0) {
				filesystem_set_sort(SORT_TYPE_SIZE_DESC);
			}

			free(s_sort);
			return 0;
		}
	} else if(strcmp(s_name, "ident") == 0) {
		ident_set_setting(name, value);
	} else if(strcmp(s_name, "colors") == 0) {
		colors_set_setting(name, value);
	}

	return 1;
}

/*
 * ----------------
 *
 * Public functions
 *
 * ----------------
 */

struct config *config_get_conf() {
	return app_conf;
}

void config_cleanup() {
	if(app_conf->sites != NULL) {
		struct site_config *p = app_conf->sites;
		struct site_config *t;

		while(p != NULL) {
			t = p;
			p = p->next;
			free(t);
		}
	}

	free(app_conf);
}

struct site_config *get_site_config(uint32_t id) {
	struct site_config *p = app_conf->sites;

	while(p != NULL) {
		if(p->id == id) {
			return p;
		}

		p = p->next;
	}

	return NULL;
}

struct site_config *get_site_config_by_name(const char *name) {
	struct site_config *p = app_conf->sites;

	while(p != NULL) {
		if(strcasecmp(p->name, name) == 0) {
			return p;
		}

		p = p->next;
	}

	return NULL;
}

void add_site_config(struct site_config *conf) {
	if(app_conf->sites == NULL) {
		app_conf->sites = conf;
		return;
	}

	struct site_config *p = app_conf->sites;

	while(p->next != NULL) {
		p = p->next;
	}
	p->next = conf;
}


bool config_read(char *path) {
	if(!init()) {
		return false;
	}

	app_conf->sites = read_site_config_file("noob");

	if (ini_parse(path, ini_read_handler, NULL) < 0) {
		return false;
	}

	//print_site_configs();

	return true;
}

bool write_site_config_file(struct site_config *sites, const char *key) {
	//count num sites
	uint32_t n_sites = 0;
	struct site_config *s = sites;

	while(s != NULL) {
		n_sites++;

		s = s->next;
	}


	if(n_sites == 0) {
		return false;
	}

	struct sites_file_head head;

	size_t head_len = sizeof(struct sites_file_head);
	size_t site_len = sizeof(struct site_config);

	strlcpy(head.magic, "MFXP", 5);
	head.n_sites = n_sites;
	head.reserved1 = 0;
	head.reserved2 = 0;
	head.reserved3 = 0;
	head.reserved4 = 0;

	char *dbpath = expand_home_path(SITE_CONFIG_FILE_PATH);

	FILE *fp = fopen(dbpath, "wb");

	free(dbpath);

	if(fp == NULL) {
		return false;
	}

	//calc size of data we need
	int out_len = head_len + n_sites * site_len;
	uint8_t *out_buf = malloc(out_len);
	size_t ofs = head_len;

	memcpy(out_buf, &head, head_len);

	s = sites;

	while(s != NULL) {
		memcpy(out_buf+ofs, s, site_len);
		s = s->next;
		ofs += site_len;
	}

	//encrypt our buffer
	uint8_t *enc_buf = aes_encrypt(out_buf, &out_len);

	if(fwrite(enc_buf, 1, out_len, fp) != out_len) {
		fclose(fp);
		free(enc_buf);
		free(out_buf);
		return false;
	}

	free(enc_buf);
	free(out_buf);
	fclose(fp);
	return true;
}

struct site_config *read_site_config_file(const char *key) {
	char *dbpath = expand_home_path(SITE_CONFIG_FILE_PATH);

	//if file dont exist, simply return
	if(!file_exists(dbpath)) {
		free(dbpath);
		return NULL;
	}

	FILE *fp = fopen(dbpath, "rb");

	free(dbpath);

	if(fp == NULL) {
		return NULL;
	}

	struct sites_file_head head;
	size_t head_len = sizeof(struct sites_file_head);
	size_t site_len = sizeof(struct site_config);

	size_t out_len;


	fseek(fp, 0L, SEEK_END);
	int file_sz = ftell(fp);
	rewind(fp);

	uint8_t *enc_buf = malloc(file_sz);


	if((out_len = fread(enc_buf, 1, file_sz, fp)) != file_sz) {
		//printf("failed reading sitedb file.\n");
		free(enc_buf);
		printf("failed to read sitesdb..\n");
		fclose(fp);
		return NULL;
	}

	fclose(fp);

	uint8_t *dec_buf = aes_decrypt(enc_buf, &file_sz);

	free(enc_buf);

	memcpy(&head, dec_buf, head_len);

	if(strcmp(head.magic, "MFXP") != 0) {
		printf("bad encryption key, or sitedb is not a valid sitedb file!\n");
		free(dec_buf);
		exit(1);
	}

	if(head.n_sites == 0) {
		free(dec_buf);
		return NULL;
	}

	int ofs = head_len;
	struct site_config *sites = NULL;

	for(int i = 0; i < head.n_sites; i++) {
		struct site_config *new_site = malloc(site_len);

		memcpy(new_site, dec_buf+ofs, site_len);

		ofs += site_len;
		new_site->next = NULL;

		if(sites == NULL) {
			sites = new_site;
		} else {
			new_site->next = sites;
			sites = new_site;
		}
	}

	free(dec_buf);
	return sites;
}
