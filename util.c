#include "util.h"

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

char *path_append_file(char *path, char *file) {
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

char *path_append_dir(char *path, char *dir) {
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

double calc_transfer_speed(struct timeval *start, struct timeval *end, uint64_t size) {
	uint64_t msec_start = start->tv_sec * 1000000 + start->tv_usec; 
	uint64_t msec_end = end->tv_sec * 1000000 + end->tv_usec;
	uint64_t diff_msec = msec_end - msec_start;

	double diff_sec = (double)diff_msec / 1000000.0f;
	double bps = (double)size / diff_sec;

	return bps;
}

char *s_get_speed(double speed) {
	char *out = malloc(20);
	char *unit = malloc(5);

	snprintf(unit, 5, "B/s");

	if(speed > 1024.0) {
		snprintf(unit, 5, "KB/s");
		speed /= 1024.0;

		if(speed > 1024.0) {
			snprintf(unit, 5, "MB/s");
			speed /= 1024.0;

			if(speed > 1024.0) {
				snprintf(unit, 5, "GB/s");
				speed /= 1024.0;
			}
		}
	}

	snprintf(out, 20, "%.2f%s", speed, unit);
	free(unit);

	return out;
}

char *s_calc_transfer_speed(struct timeval *start, struct timeval *end, uint64_t size) {
	return s_get_speed(calc_transfer_speed(start, end, size));
}
