#include "util.h"

void str_ltrim(char *s) {
	int shift_n = 0;
	char *ps = s;
	size_t len = strlen(s);

	if(len == 0) {
		return;
	}

	while(*ps <= 0x20) {
		shift_n++;
		ps++;
	}

	memmove(s, s+shift_n, len-shift_n+1);
}

void str_rtrim(char *s) {
	size_t len = strlen(s);
	
	if(len == 0) {
		return;
	}
	
	s += len-1;
	while(*s <= 0x20) {
		*s = '\0';
		s--;
	}
}

void str_trim(char *s) {
	str_ltrim(s);
	str_rtrim(s);
}

void str_rtrim_slash(char *s) {
	size_t len = strlen(s);

	str_trim(s);

	if(len == 0) {
		return;
	}

	if(s[len-1] == '/') {
		s[len-1] = '\0';
	}
}

char *str_get_path(char *s) {
	char *d = strdup(s);
	char *r = strdup(dirname(d));
	free(d);

	size_t len = strlen(r);

	if(r[len-1] != '/') {
		char *nr = malloc(len+2);
		snprintf(nr, len+2, "%s/", r);
		free(r);
		r = nr;
	}

	//if unix shell ~ path, expand it
	if(r[0] == '~') {
		wordexp_t exp_result;
		wordexp(r, &exp_result, 0);

		free(r);
		r = strdup(exp_result.we_wordv[0]);
	}

	return r;
}

char *str_get_file(char *s) {
	char *d = strdup(s);
	char *r = strdup(basename(d));
	free(d);

	return r;
}

bool file_exists (char *filename) {
	struct stat buffer;   
	return (stat(filename, &buffer) == 0);
}
