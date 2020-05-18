#include "thread.h"

void *thread_ident(void *ptr) {
	ident_start();
	return 0;
}

void *thread_indicator(void *ptr) {
	indicator_start();

	return 0;
}

uint32_t thread_gen_id() {
	static uint32_t id = 100;
	return id++;
}

void thread_ui_handle_event(struct msg *m) {
	struct ui_log *d = m->data;

	switch(m->event) {
	case EV_UI_LOG:
		usleep(100); //prevent ui bugs
	
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
			case LOG_T_D:
				printf(TCOL_BLUE "[data]:\n" TCOL_RESET);
				break;
		}

		printf("%s", d->str);

		rl_restore_prompt();
		rl_replace_line(save_rl_line, 0);
		rl_redisplay();
		free(save_rl_line);

		if(d->type == LOG_T_D) {
			free(d->str);
		}
		break;
	case EV_UI_RM_SITE: ;
		struct site_pair *pair = site_get_current_pair();
		if((pair->left != NULL) && (pair->left->thread_id == m->from_id)) {
			pair->left = NULL;
		}

		if((pair->right != NULL) && (pair->right->thread_id == m->from_id)) {
			pair->right = NULL;
		}

		char *s_prefix = generate_ui_prompt(' ', ' ');
		rl_set_prompt(s_prefix);
		rl_redisplay();
		free(s_prefix);

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
	switch(m->event) {
	case EV_SITE_LS:
		if(!s->ls_do_cache && !ftp_ls(s)) {
			log_ui(s->thread_id, LOG_T_E, "Error listing directory\n");
			break;
		}

		struct file_item *fl = s->cur_dirlist;
		log_ui(s->thread_id, LOG_T_I, TCOL_GREEN "[%s]:\n" TCOL_RESET, s->current_working_dir);

		const char *color = __color_white;
		while(fl != NULL) {

			//TODO: clean up
			if(fl->skip) {
				color = colors_get_conf()->skip_file;
			} else if(fl->hilight) {
				color = colors_get_conf()->hilight_file;
			} else {
				switch(fl->file_type) {
				case FILE_TYPE_FILE:
					color = colors_get_conf()->file_type_file;
					break;
				case FILE_TYPE_DIR:
					color = colors_get_conf()->file_type_dir;
					break;
				case FILE_TYPE_LINK:
					color = colors_get_conf()->file_type_symlink;
					break;
				}
			}

			log_ui(s->thread_id, LOG_T_I, generate_ui_dirlist_item(color, fl));

			fl = fl->next;
		}
		break;
	case EV_SITE_CWD:
		if(m->data == NULL) {
			printf("EV_SITE_CWD: bad path\n");
			break;
		}

		/*int cmd_len = strlen((char *)m->data) + 7;
		char *s_cmd = malloc(cmd_len);

		snprintf(s_cmd, cmd_len, "CWD %s\r\n", (char *)m->data);

		control_send(s, s_cmd);
		code = control_recv(s);

		if(code != 250) {*/
		if(!ftp_cwd(s, (char *)m->data)) {
			log_ui(s->thread_id, LOG_T_E, "Error changing directory\n");
			break;
		}

		if(!ftp_ls(s)) {
			log_ui(s->thread_id, LOG_T_E, "Error listing directory\n");
			break;
		}

		//flag to just read cache for next ls cmd
		s->ls_do_cache = true;

		//if conf set, list dir
		if(config_get_conf()->show_dirlist_on_cwd) {
			cmd_execute(s->thread_id, EV_SITE_LS, NULL);
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
		str_rtrim_slash((char *)m->data);
		char *p_dir = str_get_path((char *)m->data);
		char *p_filename = str_get_file((char *)m->data);
		//printf("<<%s>>%s<<", p_dir, p_filename);
		struct file_item *local_file = find_local_file(p_dir, p_filename);

		if(local_file == NULL) {
			log_ui(s->thread_id, LOG_T_E, "%s: no such file exists!\n", p_filename);
			free(local_file);
			break;
		}

		uint32_t filetype = local_file->file_type;
		free(local_file);

		if(filetype == FILE_TYPE_LINK) {
			log_ui(s->thread_id, LOG_T_W, "%s: no support for uploading symlinks yet\n", p_filename);
			break;
		} else if(filetype == FILE_TYPE_FILE) {
			if(!transfer_succ(ftp_put(s, p_filename, p_dir, s->current_working_dir))) {
				log_ui(s->thread_id, LOG_T_E, "%s: upload failed!\n", p_filename);
				break;
			}
		} else if(filetype == FILE_TYPE_DIR) {
			if(!transfer_succ(ftp_put_recursive(s, p_filename, p_dir, s->current_working_dir))) {
				log_ui(s->thread_id, LOG_T_E, "%s: recursive upload failed!\n", p_filename);
				break;
			}
		} else {
			log_ui(s->thread_id, LOG_T_E, "%s: unknown file type!\n", p_filename);
			break;
		}

		log_ui(s->thread_id, LOG_T_I, "%s: upload complete\n", p_filename);
		free(p_dir);
		free(p_filename);
		break;
	case EV_SITE_GET:
		if(m->data == NULL) {
			printf("EV_SITE_GET: bad path\n");
			break;
		}

		str_rtrim_slash((char *)m->data);
		struct file_item *file = find_file(s->cur_dirlist, (char *)m->data);

		if(file == NULL) {
			log_ui(s->thread_id, LOG_T_E, "%s: no such file exists!\n", (char *)m->data);
			break;
		}

		if(file->file_type == FILE_TYPE_LINK) {
			log_ui(s->thread_id, LOG_T_W, "%s: no support for downloading symlinks yet\n", (char *)m->data);
			break;
		} else if(file->file_type == FILE_TYPE_FILE) {
			if(!transfer_succ(ftp_get(s, (char *)m->data, "./", s->current_working_dir))) {
				log_ui(s->thread_id, LOG_T_E, "%s: download failed!\n", (char *)m->data);
				break;
			}
		} else if(file->file_type == FILE_TYPE_DIR) {
			if(!transfer_succ(ftp_get_recursive(s, (char *)m->data, "./", s->current_working_dir))) {
				log_ui(s->thread_id, LOG_T_E, "%s: recursive download failed!\n", (char *)m->data);
				break;
			}
		} else {
			log_ui(s->thread_id, LOG_T_E, "%s: unknown file type!\n", (char *)m->data);
			break;
		}

		log_ui(s->thread_id, LOG_T_I, "%s: download complete\n", (char *)m->data);

		break;

	case EV_SITE_FXP: ;
		struct fxp_arg *farg = (struct fxp_arg*)m->data;

		if(farg->filename == NULL) {
			printf("EV_SITE_GET: bad path\n");
			break;
		}

		str_rtrim_slash(farg->filename);
		struct file_item *fxp_file = find_file(s->cur_dirlist, farg->filename);

		if(fxp_file == NULL) {
			log_ui(s->thread_id, LOG_T_E, "%s: no such file exists!\n", farg->filename);
			break;
		}

		if(fxp_file->file_type == FILE_TYPE_LINK) {
			log_ui(s->thread_id, LOG_T_W, "%s: no support for fxping symlinks yet\n", farg->filename);
			break;
		} else if(fxp_file->file_type == FILE_TYPE_FILE) {
			if(!transfer_succ(fxp(s, farg->dst, farg->filename, s->current_working_dir, farg->dst->current_working_dir))) {
				log_ui(s->thread_id, LOG_T_E, "%s: fxp failed!\n", farg->filename);
				break;
			}
		} else if(fxp_file->file_type == FILE_TYPE_DIR) {
			if(!transfer_succ(fxp_recursive(s, farg->dst, farg->filename, s->current_working_dir, farg->dst->current_working_dir))) {
				log_ui(s->thread_id, LOG_T_E, "%s: recursive fxp failed!\n", farg->filename);
				break;
			}
		} else {
			log_ui(s->thread_id, LOG_T_E, "%s: unknown file type!\n", farg->filename);
			break;
		}

		log_ui(s->thread_id, LOG_T_I, "%s: fxp complete\n", farg->filename);

		free(farg);
		break;
	case EV_SITE_RM:
		if(m->data == NULL) {
			printf("EV_SITE_PUT: bad path\n");
			break;
		}

		str_rtrim_slash((char *)m->data);
		struct file_item *rm_file = find_file(s->cur_dirlist, (char *)m->data);
                                                                              
		if(rm_file == NULL) {
			log_ui(s->thread_id, LOG_T_E, "%s: no such file exists!\n", (char *)m->data);
			break;
		}

		if(rm_file->file_type == FILE_TYPE_LINK) {
			log_ui(s->thread_id, LOG_T_W, "%s: no support for deleting symlinks yet\n", (char *)m->data);
		} else if(rm_file->file_type == FILE_TYPE_FILE) {
			int del_len = strlen((char *)m->data) + 8;
			char *s_del = malloc(del_len);

			snprintf(s_del, del_len, "DELE %s\r\n", (char *)m->data);

			control_send(s, s_del);

			if(control_recv(s) != 250) {
				log_ui(s->thread_id, LOG_T_E, "%s: error deleting file!\n", (char *)m->data);
			} else {
				log_ui(s->thread_id, LOG_T_I, "%s: file has been deleted!\n", (char *)m->data);
			}

			free(s_del);
		} else if(rm_file->file_type == FILE_TYPE_DIR) {
			int rmd_len = strlen((char *)m->data) + 7;
			char *s_rmd = malloc(rmd_len);

			snprintf(s_rmd, rmd_len, "RMD %s\r\n", (char *)m->data);

			control_send(s, s_rmd);

			if(control_recv(s) != 250) {
				log_ui(s->thread_id, LOG_T_E, "%s: error deleting directory!\n", (char *)m->data);
			} else {
				log_ui(s->thread_id, LOG_T_I, "%s: directory has been deleted!\n", (char *)m->data);
			}

			free(s_rmd);
		}
		break;
	case EV_SITE_SITE:
		if(m->data == NULL) {
			printf("EV_SITE_SITE: no command specified.\n");
			break;
		}

		int site_len = strlen((char *)m->data) + 8;
		char *s_site = malloc(site_len);
		snprintf(s_site, site_len, "SITE %s\r\n", (char *)m->data);
		
		control_send(s, s_site);	
    	control_recv(s);

		log_ui(s->thread_id, LOG_T_I, "SITE Response:\n%s", s->last_recv);
		break;

	case EV_SITE_QUOTE:
		if(m->data == NULL) {
			printf("EV_SITE_QUOTE: no command specified.\n");
			break;
		}

		int qt_len = strlen((char *)m->data) + 3;
		char *s_qt = malloc(qt_len);
		snprintf(s_qt, qt_len, "%s\r\n", (char *)m->data);
		
		control_send(s, s_qt);	
    	control_recv(s);

		log_ui(s->thread_id, LOG_T_I, "Raw response:\n%s", s->last_recv);
		break;

	case EV_SITE_MKDIR:
		if(m->data == NULL) {
			printf("EV_SITE_MKDIR: no dir specified.\n");
			break;
		}

		if(ftp_mkd(s, (char *)m->data)) {
			log_ui(s->thread_id, LOG_T_I, "%s: dir created\n", (char *)m->data);
		} else {
			log_ui(s->thread_id, LOG_T_E, "%s: error creating dir\n", (char *)m->data);
		}
		break;
	case EV_SITE_VIEW_NFO:
		if(m->data == NULL) {
			printf("EV_SITE_VIEW_NFO: bad path\n");
			break;
		}

		str_rtrim_slash((char *)m->data);
		struct file_item *nfile = find_file(s->cur_dirlist, (char *)m->data);

		if(nfile == NULL) {
			log_ui(s->thread_id, LOG_T_E, "%s: no such file exists!\n", (char *)m->data);
			break;
		}

		if(nfile->file_type == FILE_TYPE_FILE) {
			if(nfile->size > NFO_DL_MAX_SZ) {
				log_ui(s->thread_id, LOG_T_E, "%s: exceeds max size of %d bytes!\n", (char *)m->data, NFO_DL_MAX_SZ);
				break;
			}

			if(!ftp_get(s, (char *)m->data, "/tmp/", s->current_working_dir)) {
				log_ui(s->thread_id, LOG_T_E, "%s: download failed!\n", (char *)m->data);
				break;
			}

			char *nfo_path = path_append_file("/tmp/", (char *)m->data);
			char c;
			FILE *nfd = fopen(nfo_path, "r");

			if(nfd == NULL) {
				log_ui(s->thread_id, LOG_T_E, "%s: error opening tmp file!\n", (char *)m->data);
				break;
			}

			char *nfo_data = malloc(NFO_DL_MAX_SZ+1);
			uint32_t ni = 0;

			while((c = fgetc(nfd)) != EOF) {
				nfo_data[ni] = c;
				ni++;				
			}

			nfo_data[ni] = '\0'; //ensure 0 termination
			
			fclose(nfd);
			unlink(nfo_path);
			free(nfo_path);
			log_ui(s->thread_id, LOG_T_D, "%s\n", nfo_data);
		} else {
			log_ui(s->thread_id, LOG_T_E, "%s: invalid file type!\n", (char *)m->data);
			break;
		}

		break;
	}

	//always invalidate the ls cache after a non cwd cmd
	if(m->event != EV_SITE_CWD) {
		s->ls_do_cache = false;
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

		return 0;
	} else {
		site->ls_do_cache = true;
		log_ui(site->thread_id, LOG_T_I, "Connected to %s\n", site->name);

		if(config_get_conf()->show_dirlist_on_cwd) {
			cmd_execute(site->thread_id, EV_SITE_LS, NULL);
		}
	}

	//if(!ftp_ls(site)) {
	//	log_ui(site->thread_id, LOG_T_E, "Error listing directory\n");
	//}

	//thread main loop
	while(1) {
		struct msg *m = msg_read(site->thread_id);

		if(m != NULL) {
			thread_site_handle_event(m, site);
		}
	}

	return 0;
}
