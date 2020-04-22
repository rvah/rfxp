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

bool file_exists (char *filename) {
	struct stat buffer;   
	return (stat(filename, &buffer) == 0);
}
