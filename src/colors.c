#include "colors.h"

static struct color_config *__color_conf = NULL;

/*
 * ----------------
 *
 * Private functions
 *
 * ----------------
 */

static void init() {
	__color_conf = malloc(sizeof(struct color_config));

	__color_conf->skip_file = __color_red;
	__color_conf->hilight_file = __color_yellow;
	__color_conf->file_type_file = __color_pink;
	__color_conf->file_type_dir = __color_green;
	__color_conf->file_type_symlink = __color_cyan;
}

/*
 * ----------------
 *
 * Public functions
 *
 * ----------------
 */

struct color_config *colors_get_conf() {
	return __color_conf;
}

void colors_set_setting(const char *name, const char *value) {
	if(__color_conf == NULL) {
		init();
	}

	char *v = strdup(value);
	char *n = strdup(name);
	str_trim(v);
	str_trim(n);

	const char *color = __color_white;

	//valid colors: red, green, blue, pink, white, cyan, yellow
	if(strcmp(v, "red") == 0) {
		color = __color_red;
	} else if(strcmp(v, "green") == 0) {
		color = __color_green;
	} else if(strcmp(v, "blue") == 0) {
		color = __color_blue;
	} else if(strcmp(v, "pink") == 0) {
		color = __color_pink;
	} else if(strcmp(v, "cyan") == 0) {
		color = __color_cyan;
	} else if(strcmp(v, "yellow") == 0) {
		color = __color_yellow;
	}

	if(strcmp(n, "skip_file") == 0) {
		__color_conf->skip_file = color;
	} else if(strcmp(n, "hilight_file") == 0) {
		__color_conf->hilight_file = color;
	} else if(strcmp(n, "file_type_file") == 0) {
		__color_conf->file_type_file = color;
	} else if(strcmp(n, "file_type_dir") == 0) {
		__color_conf->file_type_dir = color;
	} else if(strcmp(n, "file_type_symlink") == 0) {
		__color_conf->file_type_symlink = color;
	}

	free(n);
	free(v);
}
