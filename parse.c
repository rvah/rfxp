#include "parse.h"

//227 Entering Passive Mode (127,0,0,1,219,3)
bool parse_pasv(const char *s_in, char *out_addr, uint32_t *out_port, char *out_unparsed) {
	if((s_in == NULL) || (strlen(s_in) == 0)) {
		return false;
	}

	char *d_in = strdup(s_in);
	char *in = d_in;

	//find start
	bool found_start = false;

	while(*in != '\0') {
		if(*in == '(') {
			found_start = true;
			in++;
			break;
		}
		in++;
	}

	if(!found_start) {
		free(d_in);
		return false;
	}

	//remove trailing )
	size_t len = strlen(in);
	for(int i = len; i > 0; i--) {
		if(in[i] == ')') {
			in[i] = '\0';
		}
	}

	//split into array
	uint32_t pasv_a[6];
	char *part, *save;

	sprintf(out_unparsed, "%s", in);

	for(int i = 0; i < 6; i++) {
		if(i == 0) {
			part = strtok_r(strdup(in), ",", &save);
		} else {
			part = strtok_r(NULL, ",", &save);
		}

		if(part == NULL) {
			free(d_in);
			return false;
		}

		pasv_a[i] = atoi(part);
	}

	sprintf(out_addr, "%d.%d.%d.%d", pasv_a[0], pasv_a[1], pasv_a[2], pasv_a[3]);
	*out_port = (pasv_a[4]*256)+pasv_a[5];
	free(d_in);

	return true;
}

char *parse_pwd(const char *s) {
	if(s == NULL) {
		return NULL;
	}

	//find start
	bool found_start = false;
	bool found_end = false;

	while(*s != '\0') {
		if(*s == '"') {
			found_start = true;
			break;
		}
		s++;
	}

	if(!found_start) {
		return NULL;
	}

	const char *start_p = s+1;
	int len = 0;

	//find end
	s++;
	while(*s != '\0') {
		if( (*s == '"') && (*(s-1) != '"') && (*(s-1) != '\\')) {
			found_end = true;
			break;
		}		
		s++;
		len++;
	}

	if(!found_end) {
		return NULL;
	}

	char *path = malloc(len+1);

	memcpy(path, start_p, len);
	path[len] = '\0';

	return path;
}

char *parse_file_size(uint64_t size) {
	char unit[2];
	char *out = malloc(30);

	unit[0] = 'B';
	unit[1] = '\0';

	if(size > 1024) {
		unit[0] = 'K';
		size /= 1024;

		if(size > 1024) {
			unit[0] = 'M';
			size /= 1024;

			if(size > 1024) {
				unit[0] = 'G';
				size /= 1024;
			}
		}
	}

	snprintf(out, 30, "%lu%s", size, unit);
	return out; 
}

struct linked_str_node *parse_feat(const char *in) {
	struct linked_str_node *r = NULL;

	if((strlen(in) == 0) || (in == NULL)) {
		return NULL;
	}

	char *save;
	char *line = strtok_r(strdup(in), "\n", &save);

	while(line != NULL) {
		line = strtok_r(NULL, "\n", &save);

		if(line == NULL) {
			continue;
		}

		//if line starts with numbers, strip them
		while((*line >= 0x30) && (*line <= 0x39)) {
			line++;

			//strip leading dash
			if(*line == '-') {
				line++;
			}
		}

		//trim string
		str_trim(line);

		if((line == NULL) || (strlen(line) == 0)) {
			continue;
		}

		struct linked_str_node *item = malloc(sizeof(struct linked_str_node));
		item->str = line;
		item->next = NULL;		

		if(r == NULL) {
			r = item;
		} else {
			item->next = r;
			r = item;
		}
	}

	return r;
}
