#include "thread.h"

uint32_t thread_gen_id() {
	static uint32_t id = 1;
	return id++;
}

void thread_ui_handle_event(struct msg *m) {
	struct ui_log *d = m->data;

	int save_rl_point = rl_point;
	char *save_rl_line = rl_copy_text(0, rl_end);
	rl_save_prompt();
	rl_replace_line("", 0);
	rl_redisplay();

	switch(m->event) {
	case EV_UI_LOG:
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

		printf(d->str);

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
		control_send(s, "STAT -la\n");
		code = control_recv(s);

		if(code != 213) {
			log_ui(s->thread_id, LOG_T_E, "Error listing directory\n");
			break;
		}

		struct file_item *fl = parse_list(s->last_recv);

		if(fl == NULL) {
			log_ui(s->thread_id, LOG_T_E, "Error listing directory\n");
			break;
		}

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
		//log_ui(s->thread_id, LOG_T_I, "testing ui %d msg\n", 666);
		break;
	case EV_SITE_CWD:
		if(m->data == NULL) {
			printf("EV_SITE_CWD: bad path\n");
			break;
		}

		int cmd_len = strlen((char *)m->data) + 6;
		char *s_cmd = malloc(cmd_len);

		snprintf(s_cmd, cmd_len, "CWD %s\n", (char *)m->data);
//		log_w(s_cmd);

		control_send(s, s_cmd);
		code = control_recv(s);

		if(code != 250) {
			log_ui(s->thread_id, LOG_T_E, "Error changing directory\n");
		}
		break;
	case EV_SITE_CLOSE:
		ftp_disconnect(s);
		log_ui(s->thread_id, LOG_T_I, "connection closed.\n");
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

	//thread main loop
	while(1) {
		struct msg *m = msg_read(site->thread_id);

		if(m != NULL) {
			thread_site_handle_event(m, site);
		}
	}

	return 0;
}
