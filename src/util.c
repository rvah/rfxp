#include "util.h"

char *concat_paths(const char *a, const char *b) {
	char *tmp_a = strdup(a);
	size_t len_b = strlen(b);

	str_rtrim_slash(tmp_a);

	if( (len_b > 0) && (b[0] == '/') ) {
		b++;
	}

	size_t len = snprintf(NULL, 0, "%s/%s", tmp_a, b) + 1;
	char *r = malloc(len);
	snprintf(r, len, "%s/%s", tmp_a, b);

	free(tmp_a);

	return r;
}

char *expand_home_path(const char *in) {
	char *d = strdup(in);
	str_trim(d);

	if(d[0] == '~') {
		wordexp_t exp_result;
		wordexp(d, &exp_result, 0);

		free(d);
		d = strdup(exp_result.we_wordv[0]);
	}

	return d;
}

char *expand_full_remote_path(const char *in, const char *cwd) {
	char *d = strdup(in);
	str_trim(d);

	char *p = d;

	if( (p[0] == '.') && (p[1] == '/')) {
		p += 2;
	} else if(p[0] == '/') {
		return d;
	}

	int n_len = strlen(p) + strlen(cwd) + 2;

	char *s_new = malloc(n_len);

	if(strcmp(cwd, "/") == 0) {
		snprintf(s_new, n_len, "/%s", p);
	} else {
		snprintf(s_new, n_len, "%s/%s", cwd, p);
	}

	free(d);
	return s_new;
}

char *expand_full_local_path(const char *in) {
	char *d = expand_home_path(in);

	if(d == NULL) {
		return d;
	}

	char *dir = dirname(strdup(d));
	char *file = basename(strdup(d));

	free(d);

	char *real = realpath(dir, NULL);

	if(real == NULL) {
		return NULL;
	}

	int flen = strlen(real)+strlen(file)+2;
	char *ret = malloc(flen);

	//prevent /// and // paths

	if(strcmp(real, "/") == 0) {
		if(strcmp(file, "/") == 0) {
			snprintf(ret, flen, "%s", real);
		} else {
			snprintf(ret, flen, "%s%s", real, file);
		}
	} else {
		snprintf(ret, flen, "%s/%s", real, file);
	}

	return ret;
}

bool __match_rule(const char *rule, const char *str, int ri, int si) {
	if(rule[ri]  == '\0') {
		return str[si] == '\0';
	}

	if(rule[ri] == '*') {
		while(str[si] != '\0') {
			if(__match_rule(rule, str, ri+1, si)) {
				return true;
			}
			si++;
		}

		return __match_rule(rule, str, ri+1, si);
	}

	if((rule[ri] != str[si]) && (rule[ri] != '?')) {
		return false;
	}

	return __match_rule(rule, str, ri+1, si+1);
}

bool match_rule(const char * rule, const char *str) {
	return __match_rule(rule, str, 0, 0);
}

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

void __str_rtrim(char *s, bool trim_space) {
	size_t len = strlen(s);
	
	if(len == 0) {
		return;
	}
	
	s += len-1;

	char treshold = trim_space ? 0x1F : 0x20;

	while(*s <= treshold) {
		*s = '\0';
		s--;
	}
}

void str_rtrim(char *s) {
	return __str_rtrim(s, false);
}

void str_rtrim_special_only(char *s) {
	return __str_rtrim(s, true);
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

bool file_exists(char *filename) {
	struct stat buffer;   
	return (stat(filename, &buffer) == 0);
}

char *path_append_file(const char *path, const char *file) {
	int p_len = strlen(path);
	int f_len = strlen(file);
	int new_len = p_len + f_len + 2;
	char *ret = malloc(new_len);
	strlcpy(ret, path, new_len);

	if(path[p_len-1] != '/') {
		strlcat(ret, "/", new_len);
	}

	strlcat(ret, file, new_len);
	return ret;
}

char *path_append_dir(const char *path, const char *dir) {
	char *t = path_append_file(path, dir);
	int t_len = strlen(t);

	if(t[t_len-1] != '/') {
		int new_len = t_len+2;
		char *r = malloc(new_len);
		strlcpy(r, t, new_len);
		strlcat(r, "/", new_len);
		free(t);
		return r;
	}
	return t; 
}

void str_tolower(char *s) {
	for(int i = 0; s[i]; i++) {
		s[i] = tolower(s[i]);
	}
}
