#include "ui_helpers.h"

char *generate_ui_prompt(char indicator_l, char indicator_r) {
	char *out = malloc(255);
	char *s_noname = "--";
	char *s_lname, *s_rname;
	char *s_lspeed, *s_rspeed;
	char *s_empty = "";
	bool free_ls = false;
	bool free_rs = false;


	struct site_pair *current_pair = current_pair = site_get_current_pair();

	s_lspeed = s_empty;
	s_rspeed = s_empty;

	if(current_pair->left == NULL) {
		s_lname = s_noname;
	} else {
		s_lname = current_pair->left->name;

		if(current_pair->left->busy && (current_pair->left->current_speed != 0.0f)) {
			s_lspeed = s_get_speed(current_pair->left->current_speed);
			free_ls = true;
		}
	}

	if(current_pair->right == NULL) {
		s_rname = s_noname;
	} else {
		s_rname = current_pair->right->name;

		if(current_pair->right->busy && (current_pair->right->current_speed != 0.0f)) {
			s_rspeed = s_get_speed(current_pair->right->current_speed);
			free_rs = true;
		}
	}

	snprintf(out, 254, "[%c%s %s<>%c%s %s] >> ", indicator_l, s_lname, s_lspeed, indicator_r, s_rname, s_rspeed);

	if(free_ls) {
		free(s_lspeed);
	}

	if(free_rs) {
		free(s_rspeed);
	}

	return out;
}

char *generate_ui_dirlist_item(const char *color, struct file_item *file) {
	char *fmt_remote = "%s%-12s %5s %s%s\n";
	char *fmt_local = "%s%s%5s %s%s\n";

	char *fmt = fmt_remote;

	if(file->date[0] == '\0') {
		fmt = fmt_local;
	}

	char *f_size = parse_file_size(file->size);

	uint32_t s_len = snprintf(NULL, 0, fmt, color, file->date, f_size, file->file_name, TCOL_RESET)+1;
	char *out = malloc(s_len);

	snprintf(out, s_len, fmt, color, file->date, f_size, file->file_name, TCOL_RESET);
	return out;
}
