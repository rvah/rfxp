#include "command.h"

//general commands
void cmd_help(char *line) {
	printf("<-  -  - - ----\n");
	printf("help coming soon\n");
	printf("---- - -  -  ->\n");
}

char *get_arg(char *line, int i) {
	char *save;
	strtok_r(strdup(line), " \t", &save);
	char *arg = NULL;

	for(int j = 0; j < i; j++) {
		arg = strtok_r(NULL, " \t", &save);
	}

	return arg;
}

void bad_arg(char *cmd) {
	printf("bad arg\n");
}

struct site_info *cmd_get_site(char which) {
	struct site_pair *pair = site_get_current_pair();

	if(which == 'l') {
		return pair->left;
	} else if (which == 'r') {
		return pair->right;
	}

	return NULL;
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

	struct site_info *s;

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
	struct site_info *s = cmd_get_site(which);

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
	struct site_info *s = cmd_get_site(which);
	
	if(s == NULL) {
		printf("no site connected.\n");
		return;
	}

	char *arg_path = get_arg(line, 1);

	if(arg_path == NULL) {
		bad_arg("cd");
		return;
	}

	cmd_execute(s->thread_id, EV_SITE_CWD, (void *)arg_path);	
}

void cmd_put(char *line, char which) {
	struct site_info *s = cmd_get_site(which);

	if(s == NULL) {
		printf("no site connected.\n");
		return;
	}

	char *arg_path = get_arg(line, 1);

	if(arg_path == NULL) {
		bad_arg("put");
		return;
	}

	cmd_execute(s->thread_id, EV_SITE_PUT, (void *)arg_path);
}

void cmd_get(char *line, char which) {
	struct site_info *s = cmd_get_site(which);

	if(s == NULL) {
		printf("no site connected.\n");
		return;
	}

	char *arg_path = get_arg(line, 1);

	if(arg_path == NULL) {
		bad_arg("get");
		return;
	}
	cmd_execute(s->thread_id, EV_SITE_GET, (void *)arg_path);
}

void cmd_rm(char *line, char which) {
	printf("rm %c\n", which);
}

void cmd_site(char *line, char which) {
	printf("site %c\n", which);
}

void cmd_quote(char *line, char which) {
	printf("quote %c\n", which);
}
