#include "queue.h"
#include "filesystem.h"

static struct queue_item *__transfer_queue = NULL;
static bool __queue_running = false;
static uint32_t __id = 0;

/*
 * ----------------
 *
 * Private methods
 *
 * ----------------
 */
static uint32_t gen_queue_id() {
	return __id++;
}

static struct queue_item *first_item() {
	return __transfer_queue;
}

static struct queue_item *last_item() {
	struct queue_item *p = __transfer_queue;

	if(p == NULL) {
		return NULL;
	}

	while(p->next != NULL) {
		p = p->next;
	}

	return p;
}

static char *item_info_string(struct queue_item *item) {
	char *src = NULL;
	char *dst = NULL;
	char sl[] = "LOCAL";
	char fmt[] = "<%d> %s:%s -> %s:%s";

	switch(item->type) {
	case Q_ITEM_TYPE_PUT:
		src = sl;
		dst = item->site_dst->name;
		break;
	case Q_ITEM_TYPE_GET:
		src = item->site_src->name;
		dst = sl;
		break;
	case Q_ITEM_TYPE_FXP:
		src = item->site_src->name;
		dst = item->site_dst->name;
		break;
	}

	char *src_path = path_append_file(item->path_src, item->filename);

	size_t len = snprintf(NULL, 0, fmt, item->id,
		src, src_path, dst, item->path_dst) + 1;
	char *r = malloc(len);

	snprintf(r, len, fmt, item->id, src, src_path, dst, item->path_dst);

	free(src_path);

	return r;
}

static void add_item(uint32_t type, struct site_info *site_src,
		struct site_info *site_dst, char *path_src,
		char *path_dst, char *filename, bool is_dir) {
	struct queue_item *item = malloc(sizeof(struct queue_item));

	item->id = gen_queue_id();
	item->next = NULL;
	item->type = type;
	item->status = Q_ITEM_STAT_PENDING;
	item->site_src = site_src;
	item->site_dst = site_dst;
	item->path_src = strdup(path_src);
	item->path_dst = strdup(path_dst);
	item->filename = strdup(filename);
	item->is_dir = is_dir;

	if(__transfer_queue == NULL) {
		__transfer_queue = item;
	} else {
		struct queue_item *last = last_item();
		last->next = item;
	}

	char *info = item_info_string(item);
	log_ui_i("Added to queue [%s]\n", info);
	free(info);
}

/*
 * ----------------
 *
 * Public methods
 *
 * ----------------
 */

void queue_remove(uint32_t id) {
	struct queue_item *p = __transfer_queue;
	struct queue_item *prev = NULL;

	while(p != NULL) {
		if(p->id == id) {
			if(prev == NULL) {
				if(p->next != NULL) {
					__transfer_queue = p->next;
				} else {
					__transfer_queue = NULL;
				}
			} else {
				prev->next = p->next;
			}

			free(p);
			log_ui_i("%d: item was removed from queue!\n",id);
			return;
		}

		prev = p;
		p = p->next;
	}

	log_ui_e("%d: ID does not exist in queue!\n", id);
}

void queue_print() {
	struct queue_item *p = first_item();

	if(p == NULL) {
		log_ui_e("Queue is empty!\n");
		return;
	}

	while(p != NULL) {
		char *s = item_info_string(p);
		log_ui_i("[%s]\n", s);
		free(s);
		p = p->next;
	}
}

bool queue_running() {
	return __queue_running;
}

void queue_execute() {
	__queue_running = true;
	log_ui_i("Starting queue..\n");

	if(__transfer_queue == NULL) {
		log_ui_e("Queue is empty!\n");
		__queue_running = false;
		return;
	}

	struct queue_item *p = first_item();
	struct queue_item *prev = NULL;

	while(p != NULL) {
		char *info = item_info_string(p);

		log_ui_i("Executing: [%s]\n", info);

		free(info);

		p->status = Q_ITEM_STAT_RUNNING;

		switch(p->type) {
		case Q_ITEM_TYPE_PUT:
			if(p->is_dir) {
				ftp_put_recursive(p->site_dst, p->filename, p->path_src,
						p->path_dst);
			} else {
				ftp_put(p->site_dst, p->filename, p->path_src, p->path_dst);
			}

			p->status = Q_ITEM_STAT_SUCCESS;
			break;
		case Q_ITEM_TYPE_GET:
			//change src path if wrong
			if(strcmp(p->path_src, p->site_src->current_working_dir) != 0) {
				if(!ftp_cwd(p->site_src, p->path_src)) {
					log_ui_e("failed to cwd.\n");
					p->status = Q_ITEM_STAT_FAILURE;
					break;
				}

				if(!ftp_ls(p->site_src)) {
					log_ui_e("could not get a dirlist.\n");
					p->status = Q_ITEM_STAT_FAILURE;
					break;
				}
			}

			if(p->is_dir) {
				ftp_get_recursive(p->site_src, p->filename, p->path_dst,
						p->path_src);
			} else {
				ftp_get(p->site_src, p->filename, p->path_dst, p->path_src);
			}

			p->status = Q_ITEM_STAT_SUCCESS;
			break;
		case Q_ITEM_TYPE_FXP:
			//change src path if wrong
			if(strcmp(p->path_src, p->site_src->current_working_dir) != 0) {
				if(!ftp_cwd(p->site_src, p->path_src)) {
					log_ui_e("failed to cwd.\n");
					p->status = Q_ITEM_STAT_FAILURE;
					break;
				}

				if(!ftp_ls(p->site_src)) {
					log_ui_e("could not get a dirlist.\n");
					p->status = Q_ITEM_STAT_FAILURE;
					break;
				}
			}

			if(p->is_dir) {
				fxp_recursive(p->site_src, p->site_dst, p->filename,
						p->path_src, p->path_dst);
			} else {
				fxp(p->site_src, p->site_dst, p->filename, p->path_src,
						p->path_dst);
			}

			p->status = Q_ITEM_STAT_SUCCESS;
			break;
		}

		//TODO: improve status + reporting
		prev = p;
		p = p->next;
		queue_remove(prev->id);
	}

	log_ui_i("Queue done!\n");
	__queue_running = false;
}

void queue_add_put(struct site_info *site, char *path_local, char *path_site) {
	char *realpath = path_expand_full_local(path_local);
	char *dir = dirname(strdup(realpath));
	//TODO: check prio
	char *t_list = filesystem_local_list(dir);
	struct file_item *files = filesystem_parse_list(t_list, LOCAL);
	files = filesystem_filter_list(files, basename(strdup(realpath)));

	free(t_list);
	free(realpath);

	if(files == NULL) {
		log_ui_e("Could not find any local files matching pattern.\n");
		return;
	}

	struct file_item *t;

	while(files != NULL) {
		add_item(Q_ITEM_TYPE_PUT, NULL, site, dir, path_site, files->file_name,
				files->file_type == FILE_TYPE_DIR);

		t = files;
		files = files->next;
		free(t);
	}
}

void queue_add_get(struct site_info *site, char *path_site, char *path_local) {
	char *dir = dirname(strdup(path_site));
	char *cwd = strdup(site->current_working_dir);

	//if not /, then trim trailing slash so we can compare..
	if(strlen(cwd) > 1) {
		str_rtrim_slash(cwd);
	}

	if(strcmp(dir, cwd) != 0) {
		if(!ftp_cwd(site, dir)) {
			log_ui_e("failed to cwd.\n");
			return;
		}

		if(!ftp_ls(site)) {
			log_ui_e("could not get a dirlist.\n");
			return;
		}
	}

	struct file_item *files = filesystem_filter_list(
			filesystem_file_item_cpy(site->cur_dirlist), basename(path_site));

	if(files == NULL) {
		log_ui_w("Could not find any remote files matching pattern.\n");
		return;
	}

	struct file_item *t;

	while(files != NULL) {
		add_item(Q_ITEM_TYPE_GET, site, NULL, dir, path_local,
				files->file_name, files->file_type == FILE_TYPE_DIR);

		t = files;
		files = files->next;
		free(t);

	}
}

void queue_add_fxp(struct site_info *site_src, struct site_info *site_dst,
		char *path_src, char *path_dst) {
	char *dir = dirname(strdup(path_src));
	char *cwd = strdup(site_src->current_working_dir);

	//if not /, then trim trailing slash so we can compare..
	if(strlen(cwd) > 1) {
		str_rtrim_slash(cwd);
	}

	if(strcmp(dir, cwd) != 0) {
		if(!ftp_cwd(site_src, dir)) {
			log_ui_e("failed to cwd.\n");
			return;
		}

		if(!ftp_ls(site_src)) {
			log_ui_e("could not get a dirlist.\n");
			return;
		}
	}

	struct file_item *files = filesystem_filter_list(
			filesystem_file_item_cpy(site_src->cur_dirlist),
			basename(path_src));

	if(files == NULL) {
		log_ui_w("Could not find any remote files matching pattern.\n");
		return;
	}

	struct file_item *t;

	while(files != NULL) {
		add_item(Q_ITEM_TYPE_FXP, site_src, site_dst, dir, path_dst,
				files->file_name, files->file_type == FILE_TYPE_DIR);

		t = files;
		files = files->next;
		free(t);

	}
}
