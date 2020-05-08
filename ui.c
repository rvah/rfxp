#include "ui.h"

void ui_init() {

}

/*
ls
ref
cd dir
put dir/file
get dir/file
rm dir/file
site
quote
*/

char *commands[] = {
	"help",
	"open",
	"close",
	"lls","rls",
	"lref","rref",
	"lcd","rcd",
	"lput","rput",
	"lget","rget",
	"lrm","rrm",
	"lsite","rsite",
	"lquote","rquote",
	"lopen","ropen",
	"lfxp","rfxp",
	"lmkdir","rmkdir",
	"quit","exit",
	NULL
};


char *command_name_generator(const char *text, int state) {
	static int list_index, len;
	char *name;

	if (!state) {
		list_index = 0;
		len = strlen(text);
	}

	while ((name = commands[list_index++])) {
		if (strncmp(name, text, len) == 0) {
			return strdup(name);
		}
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
	char *save;
	char *item = strtok_r(strdup(line), " \t", &save);
	
	if(item == NULL) {
		return;
	}

	//check normal commands
	if(strcmp(item, "help") == 0) {
		cmd_help(line);
		return;
	} else if(strcmp(item, "open") == 0) {
		cmd_open(line, ' ');
		return;
	} else if(strcmp(item, "close") == 0) {
		cmd_close(line, ' ');
		return;
	} else if((strcmp(item, "quit") == 0) || (strcmp(item, "exit") == 0)) {
		cmd_quit(line, ' ');
		return;
	}

	char which = item[0];

	item++;

	//check r/l commands
	if (strcmp(item, "ls") == 0) {
		cmd_ls(line, which);
		return;
	} else if(strcmp(item, "ref") == 0) {
		cmd_ref(line, which);
		return;
	} else if(strcmp(item, "cd") == 0) {
		cmd_cd(line, which);
		return;
	} else if(strcmp(item, "put") == 0) {
		cmd_put(line, which);
		return;
	} else if(strcmp(item, "get") == 0) {
		cmd_get(line, which);
		return;
	} else if(strcmp(item, "rm") == 0) {
		cmd_rm(line, which);
		return;
	} else if(strcmp(item, "site") == 0) {
		cmd_site(line, which);
		return;
	} else if(strcmp(item, "quote") == 0) {
		cmd_quote(line, which);
		return;
	} else if(strcmp(item, "open") == 0) {
		cmd_open(line, which);
		return;
	} else if(strcmp(item, "close") == 0) {
		cmd_close(line, which);
		return;
	} else if(strcmp(item, "fxp") == 0) {
		cmd_fxp(line, which);
		return;
	} else if(strcmp(item, "mkdir") == 0) {
		cmd_mkdir(line, which);
		return;
	}

	printf("Err: bad command.\n");


	/*while(line != NULL) {


		line = strtok_r(NULL, "\n", &save);
	}*/
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
