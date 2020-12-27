#include "command.h"

/*
 * ----------------
 *
 * Private functions
 *
 * ----------------
 */

char *get_arg(char *line, int i) {
	char *save;
	strtok_r(strdup(line), " \t", &save);
	char *arg = NULL;

	for(int j = 0; j < i; j++) {
		arg = strtok_r(NULL, " \t", &save);
	}

	return arg;
}

char *get_arg_full(char *line, int i) {
	char *save;
	strtok_r(strdup(line), " \t", &save);

	for(int j = 0; j < (i-1); j++) {
		strtok_r(NULL, " \t", &save);
	}

	//trim
	str_trim(save);

	return save;
}

void bad_arg(char *cmd) {
	printf("bad arg\n");
}

struct site_info *get_site(char which) {
	struct site_pair *pair = site_get_current_pair();

	if(which == 'l') {
		return pair->left;
	} else if (which == 'r') {
		return pair->right;
	}

	return NULL;
}

/*
 * ----------------
 *
 * Public functions
 *
 * ----------------
 */

//general commands
void cmd_help(char *line) {
	printf("\nGeneral commands:\n\n");

	printf("cd <dir>\t\t\t\topen local directory\n");
//	printf("close\t\t\t\tclose both sites in pair\n");
	printf("exit/quit\t\t\t\tclose application\n");
	printf("help\t\t\t\t\tshow this help(!)\n");
	printf("log <num lines>\t\t\t\tshow last n lines from log\n");
	printf("ls\t\t\t\t\tlist current local working directory\n");
//	printf("mkdir\t\t\t\tcreate local directory\n");
//	printf("open <site1,site2>\t\topen site pair\n");
	printf("rm <dir/file>\t\t\t\tdelete local file or directory\n");
	printf("ssort\t\t\t\t\tsort according to size, run once for ASC, twice for DESC\n");
	printf("tsort\t\t\t\t\tsort according to time, run once for ASC, twice for DESC\n");
	printf("nsort\t\t\t\t\tsort according to file/dirname, run once for ASC, twice for DESC\n");
	printf("qx\t\t\t\t\texecute queue\n\n");

	printf("Site manager commands:\n\n");
	printf("sm add <name> <host:port> <user> <pass>\tadd site\n");
	printf("sm rm <site>\t\t\t\tremove site\n");
	printf("sm ls\t\t\t\t\tlist all sites\n");
	printf("sm ls <site>\t\t\t\tshow site details\n\n");

	printf("Site specific commands:\n");
	printf("- prefix command with either 'l' or 'r' to run on left or right site.\n");
	printf("- example: lcd Test_Rls-BLAH/\n\n");

	printf("cd <dir>\t\t\t\topen directory on site\n");
	printf("close\t\t\t\t\tclose site connection\n");
	printf("fxp <dir/file>\t\t\t\tfxp file or directory to the other site in the pair\n");
	printf("get <dir/file>\t\t\t\tdownload file or directory\n");
	printf("ls\t\t\t\t\tlist current working directory on site\n");
	printf("mkdir <dir>\t\t\t\tcreate dir on site\n");
	printf("nfo <file>\t\t\t\tview remote nfo/sfv/txt/diz\n");
	printf("open <site name>\t\t\topen connection to site\n");
	printf("put <dir/file>\t\t\t\tupload file or directory\n");
	printf("quote <raw cmd>\t\t\t\tsend raw command string to site\n");
	printf("rm <file/dir>\t\t\t\tdelete file or directory on site\n");
	printf("site <cmd>\t\t\t\tsend SITE command\n");
	printf("qput <dir/file>\t\t\t\tqueue file/dir for upload\n");
	printf("qget <dir/file>\t\t\t\tqueue file/dir for download\n");
	printf("qfxp <dir/file>\t\t\t\tqueue file/dir for fxp\n\n");
}



void cmd_execute(uint32_t thread_id, uint32_t event, void *data) {
	struct msg *m = malloc(sizeof(struct msg));
	m->to_id = thread_id;
	m->event = event;
	m->data = data;
	msg_send(m);
}

/*
	command args: open <all>
*/
void cmd_open(char *line, char which) {
	char *arg_site = get_arg(line, 1);

	if(which == ' ') {
		printf("not implemented.\n");
		return;
	}

	if(arg_site == NULL) {
		bad_arg("open");
		return;
	}

	struct site_config *site_conf = get_site_config_by_name(arg_site);

	if(site_conf == NULL) {
		printf("could not find site %s.\n", arg_site);
		return;
	}

	struct site_info *s = site_init(
		site_conf->name,
		site_conf->host,
		site_conf->port,
		site_conf->user,
		site_conf->pass,
		true
	);

	struct site_pair *pair = site_get_current_pair();

	if(which == 'l') {
		pair->left = s;
	} else if(which == 'r') {
		pair->right = s;
	}

	pthread_create(&s->thread, NULL, thread_site, s);
}

/*
	command args: close <all>
*/
void cmd_close(char *line, char which) {
	if(which == ' ') {
		printf("not implemented.\n");
		return;
	}

	struct site_pair *pair = site_get_current_pair();

	struct site_info *s = NULL;

	if(which == 'l') {
		s = pair->left;
	} else if (which == 'r') {
		s = pair->right;
	} else {
		// not implemented
	}

	if(s == NULL) {
		printf("no site connected.\n");
		return;
	}

	cmd_execute(s->thread_id, EV_SITE_CLOSE, NULL);

	if(which == 'l') {
		pair->left = NULL;
	} else if(which == 'r') {
		pair->right = NULL;
	}
}

//site specific commands
void cmd_ls(char *line, char which) {
	struct site_info *s = get_site(which);

	if(s == NULL) {
		printf("no site connected.\n");
		return;
	}

	cmd_execute(s->thread_id, EV_SITE_LS, NULL);
}

void cmd_ref(char *line, char which) {
	printf("ref %c\n", which);
}

void cmd_cd(char *line, char which) {
	struct site_info *s = get_site(which);

	if(s == NULL) {
		printf("no site connected.\n");
		return;
	}

	char *arg_path = get_arg_full(line, 1);

	if(arg_path == NULL) {
		bad_arg("cd");
		return;
	}

	cmd_execute(s->thread_id, EV_SITE_CWD, (void *)arg_path);
}

void cmd_put(char *line, char which) {
	struct site_info *s = get_site(which);

	if(s == NULL) {
		printf("no site connected.\n");
		return;
	}

	char *arg_path = get_arg_full(line, 1);

	if(arg_path == NULL) {
		bad_arg("put");
		return;
	}

	cmd_execute(s->thread_id, EV_SITE_PUT, (void *)arg_path);
}

void cmd_get(char *line, char which) {
	struct site_info *s = get_site(which);

	if(s == NULL) {
		printf("no site connected.\n");
		return;
	}

	char *arg_path = get_arg_full(line, 1);

	if(arg_path == NULL) {
		bad_arg("get");
		return;
	}
	cmd_execute(s->thread_id, EV_SITE_GET, (void *)arg_path);
}

void cmd_rm(char *line, char which) {
	struct site_info *s = get_site(which);

	if(s == NULL) {
		printf("no site connected.\n");
		return;
	}

	char *arg_path = get_arg_full(line, 1);

	if(arg_path == NULL) {
		bad_arg("rm");
		return;
	}

	cmd_execute(s->thread_id, EV_SITE_RM, (void *)arg_path);
}

void cmd_site(char *line, char which) {
	struct site_info *s = get_site(which);

	if(s == NULL) {
		printf("no site connected.\n");
		return;
	}

	char *arg = get_arg_full(line, 1);
	cmd_execute(s->thread_id, EV_SITE_SITE, (void *)arg);
}

void cmd_quote(char *line, char which) {
	struct site_info *s = get_site(which);

	if(s == NULL) {
		printf("no site connected.\n");
		return;
	}

	char *arg = get_arg_full(line, 1);
	cmd_execute(s->thread_id, EV_SITE_QUOTE, (void *)arg);
}

void cmd_fxp(char *line, char which) {
	struct site_info *s = NULL;
	struct site_info *d = NULL;

	struct site_pair *pair = site_get_current_pair();

	if(which == 'l') {
		s = pair->left;
		d = pair->right;
	} else {
		s = pair->right;
		d = pair->left;
	}


	if(s == NULL) {
		printf("src site not connected.\n");
		return;
	}

	if(d == NULL) {
		printf("dst site not connected.\n");
		return;
	}

	char *arg_path = get_arg_full(line, 1);

	if(arg_path == NULL) {
		bad_arg("fxp");
		return;
	}

	struct fxp_arg *fa = malloc(sizeof(struct fxp_arg));
	fa->filename = arg_path;
	fa->dst = d;

	cmd_execute(s->thread_id, EV_SITE_FXP, (void*)fa);
}

void cmd_mkdir(char *line, char which) {
	struct site_info *s = get_site(which);

	if(s == NULL) {
		printf("no site connected.\n");
		return;
	}

	char *arg = get_arg_full(line, 1);
	cmd_execute(s->thread_id, EV_SITE_MKDIR, (void *)arg);
}

void cmd_quit(char *line) {
	//TODO: proper cleanup needed :)
	exit(0);
}

void cmd_log(char *line) {
	char *arg = get_arg(line, 1);

	uint32_t n_lines = 50;

	if((arg != NULL) && (strlen(arg) > 0)) {
		str_trim(arg);

		n_lines = atoi(arg);

		//put a reasonable limit..
		if(n_lines > 10000) {
			n_lines = 10000;
		}

		if(n_lines == 0) {
			n_lines = 50;
		}
	}

	log_print(n_lines);
}

void cmd_nfo(char *line, char which) {
	struct site_info *s = get_site(which);

	if(s == NULL) {
		printf("no site connected.\n");
		return;
	}

	char *arg_path = get_arg_full(line, 1);

	if(arg_path == NULL) {
		bad_arg("nfo");
		return;
	}

	char *d = strdup(arg_path);
	str_tolower(d);

	const char *pos = strrchr(d, '.');

	if(!pos || pos == d) {
		printf("unable to read file type\n");
		return;
	}

	if( (strcmp(pos+1, "nfo") != 0) && (strcmp(pos+1, "sfv") != 0) && (strcmp(pos+1, "diz") != 0) && (strcmp(pos+1, "txt") != 0) ) {
		printf("bad filetype, supported filetypes: nfo, sfv, diz, txt\n");
		return;
	}

	free(d);

	cmd_execute(s->thread_id, EV_SITE_VIEW_NFO, (void *)arg_path);
}

void cmd_local_ls(char *line) {
	struct file_item *fl = filesystem_local_ls("./", false);
	struct file_item *prev = NULL;

	char cwd[PATH_MAX];
	getcwd(cwd, sizeof(cwd));

	log_ui(THREAD_ID_UI, LOG_T_I, TCOL_GREEN "[%s]:\n" TCOL_RESET, cwd);

	const char *color = __color_white;

	while(fl != NULL) {
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

		log_ui(THREAD_ID_UI, LOG_T_I, generate_ui_dirlist_item(color, fl));

		prev = fl;
		fl = fl->next;
		free(prev);
	}
}

void cmd_local_cd(char *line) {
	char *arg_path = get_arg_full(line, 1);

	if( (arg_path == NULL) || (strlen(arg_path) == 0)) {
		printf("bad path\n");
	}

	str_trim(arg_path);

	if(chdir(arg_path) != 0) {
		log_ui(THREAD_ID_UI, LOG_T_E,"%s: error CWD\n", arg_path);
		return;
	}

	log_ui(THREAD_ID_UI, LOG_T_I,"cwd successful.\n");

	return;
}

void cmd_local_rm(char *line) {
	printf("not implemented.\n");
	return;
}

void cmd_local_mkdir(char *line) {
	printf("not implemented.\n");
	return;
}

void cmd_sort(char *line, char type) {
	uint32_t cur_sort = filesystem_get_sort();
	uint32_t new_sort;

	const char *s_sort[] = {
		"time, ascending",
		"time, descending",
		"file/dirname, ascending",
		"file/dirname, descending",
		"size, ascending",
		"size, descending"
	};


	switch(type) {
	case 'n':
		new_sort = (cur_sort == SORT_TYPE_NAME_ASC) ? SORT_TYPE_NAME_DESC : SORT_TYPE_NAME_ASC;
		break;
	case 's':
		new_sort = (cur_sort == SORT_TYPE_SIZE_ASC) ? SORT_TYPE_SIZE_DESC : SORT_TYPE_SIZE_ASC;
		break;
	case 't':
		new_sort = (cur_sort == SORT_TYPE_TIME_ASC) ? SORT_TYPE_TIME_DESC : SORT_TYPE_TIME_ASC;
		break;
	}

	printf("sort set to: %s\n", s_sort[new_sort]);

	filesystem_set_sort(new_sort);
}

void cmd_nsort(char *line) {
	cmd_sort(line, 'n');
}

void cmd_tsort(char *line) {
	cmd_sort(line, 't');
}

void cmd_ssort(char *line) {
	cmd_sort(line, 's');
}

void cmd_sm(char *line) {
	char *cmd = get_arg(line, 1);
	char *which_site;

	if(cmd == NULL) {
		printf("bad args, please see help\n");
		return;
	}

	if(strcmp(cmd, "add") == 0) {
		//sm add <name> <host:port> <user> <pass>
		char *name = get_arg(line, 2);
		char *hport = get_arg(line, 3);
		char *user = get_arg(line, 4);
		char *pass = get_arg(line, 5);

		char *host;
		char *port;
		char *save;

		if((name == NULL) || (hport == NULL) || (user == NULL) || (pass == NULL)) {
			printf("bad args, please see help\n");
			return;
		}

		if(strstr(hport, ":") == NULL) {
			printf("bad host:port format, please fix and try again.\n");
			return;
		}

		if(get_site_config_by_name(name) != NULL) {
			printf("site with name '%s' already exists.\n", name);
			return;
		}

		host = strtok_r(hport, ":", &save);
		port = strtok_r(NULL, ":", &save);

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
		strlcpy(new_site->pass, pass, 255);
		strlcpy(new_site->user, user, 255);
		strlcpy(new_site->host, host, 255);
		strlcpy(new_site->port, port, 6);

		new_site->tls = true;
		new_site->next = NULL;

		add_site_config(new_site);

		printf("added site '%s'\n", new_site->name);

		//commit changes to disk
		if(!write_site_config_file(config_get_conf()->sites, "noob")) {
			printf("Error writing sitedb to disk!\n");
		}
	} else if(strcmp(cmd, "rm") == 0) {
		which_site = get_arg(line, 2);

		if((which_site == NULL) || (get_site_config_by_name(which_site) == NULL)) {
			printf("could not find any site called '%s'\n", which_site);
			return;
		}

		struct site_config *all = config_get_conf()->sites;
		struct site_config *prev = NULL;

		while(all != NULL) {
			if(strcasecmp(all->name, which_site) == 0) {
				if(prev == NULL) {
					config_get_conf()->sites = all->next;
				} else {
					prev->next = all->next;
				}

				printf("deleted site '%s'\n", all->name);

				free(all);
				break;
			}

			prev = all;
			all = all->next;
		}

		//commit changes to disk
		if(!write_site_config_file(config_get_conf()->sites, "noob")) {
			printf("Error writing sitedb to disk!\n");
		}
	} else if(strcmp(cmd, "ls") == 0) {
		struct site_config *s = config_get_conf()->sites;
		which_site = get_arg(line, 2);

		if(which_site == NULL) {
			if(s == NULL) {
				printf("no sites added.\n");
				return;
			}

			printf("Added sites:\n");

			while(s != NULL) {
				printf("%s", s->name);

				s = s->next;

				if(s != NULL) {
					printf(", ");
				}
			}

			printf("\n");
		} else {
			struct site_config *site = get_site_config_by_name(which_site);

			if(site == NULL) {
				printf("could not find any site called '%s'\n", which_site);
			} else {
				printf("ID: %d\n", site->id);
				printf("name: %s\n", site->name);
				printf("host: %s\n", site->host);
				printf("port: %s\n", site->port);
				printf("user: %s\n", site->user);
				printf("pass: <hidden>\n");
			}
		}
	} else {
		printf("bad sm command, please see help\n");
		return;
	}
}

void cmd_qput(char *line, char which) {
	char *arg_path = get_arg(line, 1);

	if(arg_path == NULL) {
		bad_arg("qput");
		return;
	}

	struct site_info *s = get_site(which);

	if(s == NULL) {
		printf("no site connected.\n");
		return;
	}

	char *lpath = expand_full_local_path(arg_path);

	if(lpath == NULL) {
		printf("path does not exist.\n");
		return;
	}

	queue_add_put(s, lpath, s->current_working_dir);
}

void cmd_qget(char *line, char which) {
	char *arg_path = get_arg(line, 1);

	if(arg_path == NULL) {
		bad_arg("qget");
		return;
	}

	struct site_info *s = get_site(which);

	if(s == NULL) {
		printf("no site connected.\n");
		return;
	}

	char *rpath = expand_full_remote_path(arg_path, s->current_working_dir);

	queue_add_get(s, rpath, realpath(".", NULL));
}

void cmd_qfxp(char *line, char which) {
	char *arg_path = get_arg(line, 1);

	if(arg_path == NULL) {
		bad_arg("qfxp");
		return;
	}

	struct site_info *s = NULL;
	struct site_info *d = NULL;

	struct site_pair *pair = site_get_current_pair();

	if(which == 'l') {
		s = pair->left;
		d = pair->right;
	} else {
		s = pair->right;
		d = pair->left;
	}


	if(s == NULL) {
		printf("src site not connected.\n");
		return;
	}

	if(d == NULL) {
		printf("dst site not connected.\n");
		return;
	}

	char *spath = expand_full_remote_path(arg_path, s->current_working_dir);

	queue_add_fxp(s, d, spath, d->current_working_dir);
}

void cmd_qx(char *line) {
	if(queue_running()) {
		printf("error: queue already running.\n");
		return;
	}

	pthread_t queue_thread;

	pthread_create(&queue_thread, NULL, thread_run_queue, NULL);
}

void cmd_qls(char *line) {
	queue_print();
}

void cmd_qrm(char *line) {
	char *arg_id = get_arg(line, 1);

	if(arg_id == NULL) {
		bad_arg("qrm");
		return;
	}
	queue_remove(atoi(arg_id));
}

void cmd_lls(char *line) {
	cmd_ls(line, 'l');
}

void cmd_rls(char *line) {
	cmd_ls(line, 'r');
}

void cmd_lref(char *line) {
	cmd_ref(line, 'l');
}

void cmd_rref(char *line) {
	cmd_ref(line, 'r');
}

void cmd_lcd(char *line) {
	cmd_cd(line, 'l');
}

void cmd_rcd(char *line) {
	cmd_cd(line, 'r');
}

void cmd_lput(char *line) {
	cmd_put(line, 'l');
}

void cmd_rput(char *line) {
	cmd_put(line, 'r');
}

void cmd_lget(char *line) {
	cmd_get(line, 'l');
}

void cmd_rget(char *line) {
	cmd_get(line, 'r');
}

void cmd_lrm(char *line) {
	cmd_rm(line, 'l');
}

void cmd_rrm(char *line) {
	cmd_rm(line, 'r');
}

void cmd_lsite(char *line) {
	cmd_site(line, 'l');
}

void cmd_rsite(char *line) {
	cmd_site(line, 'r');
}

void cmd_lquote(char *line) {
	cmd_quote(line, 'l');
}

void cmd_rquote(char *line) {
	cmd_quote(line, 'r');
}

void cmd_lopen(char *line) {
	cmd_open(line, 'l');
}

void cmd_ropen(char *line) {
	cmd_open(line, 'r');
}

void cmd_lclose(char *line) {
	cmd_close(line, 'l');
}

void cmd_rclose(char *line) {
	cmd_close(line, 'r');
}

void cmd_lfxp(char *line) {
	cmd_fxp(line, 'l');
}

void cmd_rfxp(char *line) {
	cmd_fxp(line, 'r');
}

void cmd_lmkdir(char *line) {
	cmd_mkdir(line, 'l');
}

void cmd_rmkdir(char *line) {
	cmd_mkdir(line, 'r');
}

void cmd_lnfo(char *line) {
	cmd_nfo(line, 'l');
}

void cmd_rnfo(char *line) {
	cmd_nfo(line, 'r');
}

void cmd_lqput(char *line) {
	cmd_qput(line, 'l');
}

void cmd_rqput(char *line) {
	cmd_qput(line, 'r');
}

void cmd_lqget(char *line) {
	cmd_qget(line, 'l');
}

void cmd_rqget(char *line) {
	cmd_qget(line, 'r');
}

void cmd_lqfxp(char *line) {
	cmd_qfxp(line, 'l');
}

void cmd_rqfxp(char *line) {
	cmd_qfxp(line, 'r');
}
