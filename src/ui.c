#include "ui.h"

/*
 * ----------------
 *
 * Private functions
 *
 * ----------------
 */

struct dict_node **__cmd_lut;
struct linked_str_node *__cmd_list;

void dict_and_list_set(struct dict_node **dict, const char *key, void *value) {
	struct linked_str_node *item = malloc(sizeof(struct linked_str_node));
	item->str = strdup(key);
	item->next = NULL;

	if(__cmd_list == NULL) {
		__cmd_list = item;
	} else {
		item->next = __cmd_list;
		__cmd_list = item;
	}

	dict_set(dict, key, value);
}

void init_command_tables() {
	__cmd_lut = dict_create();

	dict_and_list_set(__cmd_lut, "help", &cmd_help);
	dict_and_list_set(__cmd_lut, "quit", &cmd_quit);
	dict_and_list_set(__cmd_lut, "exit", &cmd_quit);
	dict_and_list_set(__cmd_lut, "log", &cmd_log);
	dict_and_list_set(__cmd_lut, "ls", &cmd_local_ls);
	dict_and_list_set(__cmd_lut, "cd", &cmd_local_cd);
	dict_and_list_set(__cmd_lut, "rm", &cmd_local_rm);
	dict_and_list_set(__cmd_lut, "mkdir", &cmd_local_mkdir);
	dict_and_list_set(__cmd_lut, "nsort", &cmd_nsort);
	dict_and_list_set(__cmd_lut, "tsort", &cmd_tsort);
	dict_and_list_set(__cmd_lut, "ssort", &cmd_ssort);
	dict_and_list_set(__cmd_lut, "sm", &cmd_sm);
	dict_and_list_set(__cmd_lut, "qx", &cmd_qx);
	dict_and_list_set(__cmd_lut, "qls", &cmd_qls);
	dict_and_list_set(__cmd_lut, "qrm", &cmd_qrm);
	dict_and_list_set(__cmd_lut, "lls", &cmd_lls);
	dict_and_list_set(__cmd_lut, "rls", &cmd_rls);
	dict_and_list_set(__cmd_lut, "lref", &cmd_lref);
	dict_and_list_set(__cmd_lut, "rref", &cmd_rref);
	dict_and_list_set(__cmd_lut, "lcd", &cmd_lcd);
	dict_and_list_set(__cmd_lut, "rcd", &cmd_rcd);
	dict_and_list_set(__cmd_lut, "lput", &cmd_lput);
	dict_and_list_set(__cmd_lut, "rput", &cmd_rput);
	dict_and_list_set(__cmd_lut, "lget", &cmd_lget);
	dict_and_list_set(__cmd_lut, "rget", &cmd_rget);
	dict_and_list_set(__cmd_lut, "lrm", &cmd_lrm);
	dict_and_list_set(__cmd_lut, "rrm", &cmd_rrm);
	dict_and_list_set(__cmd_lut, "lsite", &cmd_lsite);
	dict_and_list_set(__cmd_lut, "rsite", &cmd_rsite);
	dict_and_list_set(__cmd_lut, "lquote", &cmd_lquote);
	dict_and_list_set(__cmd_lut, "rquote", &cmd_rquote);
	dict_and_list_set(__cmd_lut, "lopen", &cmd_lopen);
	dict_and_list_set(__cmd_lut, "ropen", &cmd_ropen);
	dict_and_list_set(__cmd_lut, "lclose", &cmd_lclose);
	dict_and_list_set(__cmd_lut, "rclose", &cmd_rclose);
	dict_and_list_set(__cmd_lut, "lfxp", &cmd_lfxp);
	dict_and_list_set(__cmd_lut, "rfxp", &cmd_rfxp);
	dict_and_list_set(__cmd_lut, "lmkdir", &cmd_lmkdir);
	dict_and_list_set(__cmd_lut, "rmkdir", &cmd_rmkdir);
	dict_and_list_set(__cmd_lut, "lnfo", &cmd_lnfo);
	dict_and_list_set(__cmd_lut, "rnfo", &cmd_rnfo);
	dict_and_list_set(__cmd_lut, "lqput", &cmd_lqput);
	dict_and_list_set(__cmd_lut, "rqput", &cmd_rqput);
	dict_and_list_set(__cmd_lut, "lqget", &cmd_lqget);
	dict_and_list_set(__cmd_lut, "rqget", &cmd_rqget);
	dict_and_list_set(__cmd_lut, "lqfxp", &cmd_lqfxp);
	dict_and_list_set(__cmd_lut, "rqfxp", &cmd_rqfxp);
}

char *command_name_generator(const char *text, int state) {
	static int len;
	static struct linked_str_node *p;
	//char *name;

	if (!state) {
		p = __cmd_list;
		len = strlen(text);
	}

	//while ((name = commands[list_index++])) {
	while(p != NULL) {
		if (strncmp(p->str, text, len) == 0) {
			char *match = strdup(p->str);
			p = p->next;
			return match;
		}

		p = p->next;
	}

	return NULL;
}

char *command_arg_generator(const char *text, int state) {
	static struct file_item *cur = NULL;
	static int len;
	if(!state) {
		if(rl_line_buffer == NULL) {
			return NULL;
		}

		len = strlen(text);

		char *t = strdup(rl_line_buffer);
		str_ltrim(t);

		struct site_pair *pair = site_get_current_pair();
		struct site_info *s = NULL;

		if(t[0] == 'l') {
			s = pair->left;
		} else if(t[0] == 'r'){
			s = pair->right;
		}

		free(t);

		if(s == NULL) {
			return NULL;
		}

		if(s->cur_dirlist == NULL) {
			return NULL;
		}

		cur = s->cur_dirlist;
	}

	char *fn;

	while(cur != NULL) {
		fn = cur->file_name;
		cur = cur->next;
		if (strncmp(fn, text, len) == 0) {
			return strdup(fn);
		}
	}

	return NULL;
}

void parse_command_line(char *line) {
	//if NULL, ctrl+d probably pressed. terminate.
	if(line == NULL) {
		printf("\n"); //make it pretty!
		cmd_quit(line);
		return;
	}

	char *save;
	char *item = strtok_r(strdup(line), " \t", &save);

	if(item == NULL) {
		return;
	}


	void (*fp)(char *) = dict_get(__cmd_lut, item);

	if(fp == NULL) {
		printf("Err: bad command.\n");
		return;
	}

	(*fp)(line);
}

int get_current_arg_n() {
	char *t = strdup(rl_line_buffer);
	char *s;
	int n = 0;

	char *sp = strtok_r(t, " \r", &s);

	while(sp != NULL) {
		sp = strtok_r(NULL, " \r", &s);
		n++;
	}

	free(t);
	return n;
}

char **tab_auto_complete(const char *text, int start, int end) {
	int arg_n = get_current_arg_n();
	//printf("argn: %d\n",arg_n);
	rl_attempted_completion_over = 1;

	//if first arg, look for commands
	//if second arg, look for command compatible completions
	if(arg_n <= 1) {
		return rl_completion_matches(text, command_name_generator);
	} else {
		return rl_completion_matches(text, command_arg_generator);
	}
}

/*
 * ----------------
 *
 * Public functions
 *
 * ----------------
 */

void ui_init() {
	init_command_tables();
}

void ui_loop() {
	bool running = true;
	char *str_in; //[UI_INPUT_MAX];
	char *s_prefix;//[255];

	rl_attempted_completion_function = tab_auto_complete;

	pthread_t ui_thread;
	pthread_create(&ui_thread, NULL, thread_ui, NULL);

	pthread_t ui_ind_thread;
	pthread_create(&ui_ind_thread, NULL, thread_indicator, NULL);

	while(running) {
		s_prefix = generate_ui_prompt(' ', ' ');

		//fgets(str_in, UI_INPUT_MAX , stdin);
		str_in = readline(s_prefix);
		add_history(str_in);
		parse_command_line(str_in);
		free(str_in);
		free(s_prefix);
	}
}

void ui_close() {

}
