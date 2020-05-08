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
