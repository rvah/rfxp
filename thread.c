#include "thread.h"

uint32_t thread_gen_id() {
	static uint32_t id = 1;
	return id++;
}

void thread_ui_handle_event(struct msg *m) {
	struct ui_log *d = m->data;

	switch(m->event) {
	case EV_UI_LOG:
		usleep(100); //prevent ui bugs
	
		int save_rl_point = rl_point;
		char *save_rl_line = rl_copy_text(0, rl_end);
		rl_save_prompt();
		rl_replace_line("", 0);
		rl_redisplay();

		switch(d->type) {
			case LOG_T_E:
				printf(TCOL_RED "[!] " TCOL_RESET);
				break;
			case LOG_T_W:
				printf(TCOL_YELLOW "[w] " TCOL_RESET);
				break;
			case LOG_T_I:
				printf(TCOL_GREEN "[i] " TCOL_RESET);
				break;
		}

		printf("%s", d->str);

		rl_restore_prompt();
		rl_replace_line(save_rl_line, 0);
		rl_point = save_rl_point;
		rl_redisplay();
		free(save_rl_line);
		break;
	case EV_UI_RM_SITE: ;
		struct site_pair *pair = site_get_current_pair();
		if((pair->left != NULL) && (pair->left->thread_id == m->from_id)) {
			pair->left = NULL;
		}

		if((pair->right != NULL) && (pair->right->thread_id == m->from_id)) {
			pair->right = NULL;
		}

		break;
	}
	
	free(m);
}

void *thread_ui(void *ptr) {
	while(1) {
		struct msg *m = msg_read(THREAD_ID_UI);

		if(m != NULL) {
			thread_ui_handle_event(m);
		}
	}
	return 0;
}

void thread_site_handle_event(struct msg *m, struct site_info *s) {
	int32_t code;

	switch(m->event) {
	case EV_SITE_LS:
		if(!ftp_ls(s)) {
			log_ui(s->thread_id, LOG_T_E, "Error listing directory\n");
			break;
		}

		struct file_item *fl = s->cur_dirlist;

		while(fl != NULL) {
			switch(fl->file_type) {
			case FILE_TYPE_FILE:
				log_ui(s->thread_id, LOG_T_I, TCOL_PINK "%s\n" TCOL_RESET, fl->file_name);
				break;
			case FILE_TYPE_DIR:
				log_ui(s->thread_id, LOG_T_I, TCOL_GREEN "%s/\n" TCOL_RESET, fl->file_name);
				break;
			case FILE_TYPE_LINK:
				log_ui(s->thread_id, LOG_T_I, TCOL_CYAN "%s\n" TCOL_RESET, fl->file_name);
				break;
			}

			fl = fl->next;
		}
		break;
	case EV_SITE_CWD:
		if(m->data == NULL) {
			printf("EV_SITE_CWD: bad path\n");
			break;
		}

		int cmd_len = strlen((char *)m->data) + 6;
		char *s_cmd = malloc(cmd_len);

		snprintf(s_cmd, cmd_len, "CWD %s\n", (char *)m->data);

		control_send(s, s_cmd);
		code = control_recv(s);

		if(code != 250) {
			log_ui(s->thread_id, LOG_T_E, "Error changing directory\n");
			break;
		}

		if(!ftp_ls(s)) {
			log_ui(s->thread_id, LOG_T_E, "Error listing directory\n");
			break;
		}

		break;
	case EV_SITE_CLOSE:
		ftp_disconnect(s);
		log_ui(s->thread_id, LOG_T_I, "connection closed.\n");
		break;
	case EV_SITE_PUT:
		if(m->data == NULL) {
			printf("EV_SITE_PUT: bad path\n");
			break;
		}
		struct file_item *local_file = find_local_file("./", (char *)m->data);

		if(local_file == NULL) {
			log_ui(s->thread_id, LOG_T_E, "%s: no such file exists!\n", (char *)m->data);
			free(local_file);
			break;
		}

		uint32_t filetype = local_file->file_type;
		free(local_file);

		if(filetype == FILE_TYPE_LINK) {
			log_ui(s->thread_id, LOG_T_W, "%s: no support for uploading symlinks yet\n", (char *)m->data);
			break;
		} else if(filetype == FILE_TYPE_FILE) {
			if(!ftp_put(s, (char *)m->data, "./")) {
				log_ui(s->thread_id, LOG_T_E, "%s: upload failed!\n", (char *)m->data);
				break;
			}
		} else if(filetype == FILE_TYPE_DIR) {
			if(!ftp_put_recursive(s, (char *)m->data, "./")) {
				log_ui(s->thread_id, LOG_T_E, "%s: recursive upload failed!\n", (char *)m->data);
				break;
			}
		} else {
			log_ui(s->thread_id, LOG_T_E, "%s: unknown file type!\n", (char *)m->data);
			break;
		}

		log_ui(s->thread_id, LOG_T_I, "%s: upload complete\n", (char *)m->data);

		break;
	case EV_SITE_GET:
		if(m->data == NULL) {
			printf("EV_SITE_GET: bad path\n");
			break;
		}

		struct file_item *file = find_file(s->cur_dirlist, (char *)m->data);

		if(file == NULL) {
			log_ui(s->thread_id, LOG_T_E, "%s: no such file exists!\n", (char *)m->data);
			break;
		}

		if(file->file_type == FILE_TYPE_LINK) {
			log_ui(s->thread_id, LOG_T_W, "%s: no support for downloading symlinks yet\n", (char *)m->data);
			break;
		} else if(file->file_type == FILE_TYPE_FILE) {
			if(!ftp_get(s, (char *)m->data, "./")) {
				log_ui(s->thread_id, LOG_T_E, "%s: download failed!\n", (char *)m->data);
				break;
			}
		} else if(file->file_type == FILE_TYPE_DIR) {
			if(!ftp_get_recursive(s, (char *)m->data, "./")) {
				log_ui(s->thread_id, LOG_T_E, "%s: recursive download failed!\n", (char *)m->data);
				break;
			}
		} else {
			log_ui(s->thread_id, LOG_T_E, "%s: unknown file type!\n", (char *)m->data);
			break;
		}

		log_ui(s->thread_id, LOG_T_I, "%s: download complete\n", (char *)m->data);

		break;
	}

	//rm message since not needed anymore
	free(m);
}

void *thread_site(void *ptr) {
	struct site_info *site = (struct site_info*) ptr;
	site->thread_id = thread_gen_id();
	log_ui(site->thread_id, LOG_T_I, "Connecting to %s..\n", site->name);

	if(!ftp_connect(site)) {
		log_ui(site->thread_id, LOG_T_E, "Unable to connect to %s\n", site->name);

		struct msg *m = malloc(sizeof(struct msg));
		m->to_id = THREAD_ID_UI;
		m->from_id = site->thread_id;
		m->event = EV_UI_RM_SITE;
		msg_send(m);
	} else {
		log_ui(site->thread_id, LOG_T_I, "Connected to %s\n", site->name);
	}

	if(!ftp_ls(site)) {
		log_ui(site->thread_id, LOG_T_E, "Error listing directory\n");
	}

	//thread main loop
	while(1) {
		struct msg *m = msg_read(site->thread_id);

		if(m != NULL) {
			thread_site_handle_event(m, site);
		}
	}

	return 0;
}
