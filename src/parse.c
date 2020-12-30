#include "parse.h"

static struct dict_node **__dict_months = NULL;

struct date_info *parse_date(const char *in) {
	//supported formats
	//Aug 5 2018
	//May 13 15:47

	if(__dict_months == NULL) {
		__dict_months = dict_create();
		static uint32_t month_jan = INT_MONTH_JAN;
		static uint32_t month_feb = INT_MONTH_FEB;
		static uint32_t month_mar = INT_MONTH_MAR;
		static uint32_t month_apr = INT_MONTH_APR;
		static uint32_t month_may = INT_MONTH_MAY;
		static uint32_t month_jun = INT_MONTH_JUN;
		static uint32_t month_jul = INT_MONTH_JUL;
		static uint32_t month_aug = INT_MONTH_AUG;
		static uint32_t month_sep = INT_MONTH_SEP;
		static uint32_t month_oct = INT_MONTH_OCT;
		static uint32_t month_nov = INT_MONTH_NOV;
		static uint32_t month_dec = INT_MONTH_DEC;

		dict_set(__dict_months, "Jan", &month_jan);
		dict_set(__dict_months, "Feb", &month_feb);
		dict_set(__dict_months, "Mar", &month_mar);
		dict_set(__dict_months, "Apr", &month_apr);
		dict_set(__dict_months, "May", &month_may);
		dict_set(__dict_months, "Jun", &month_jun);
		dict_set(__dict_months, "Jul", &month_jul);
		dict_set(__dict_months, "Aug", &month_aug);
		dict_set(__dict_months, "Sep", &month_sep);
		dict_set(__dict_months, "Oct", &month_oct);
		dict_set(__dict_months, "Nov", &month_nov);
		dict_set(__dict_months, "Dec", &month_dec);
	}

	char *_in = strdup(in);
	char *save, *save2;

	str_trim(_in);

	char *s_month = strtok_r(_in, " \t", &save);
	char *s_day = strtok_r(NULL, " \t", &save);
	char *s_year_time = strtok_r(NULL, " \t", &save);

	struct date_info *ret = malloc(sizeof(struct date_info));
	ret->year = 1970;
	ret->month = INT_MONTH_JAN;
	ret->day = 1;
	ret->hour = 0;
	ret->minute = 0;

	if((s_month == NULL) || (s_day == NULL) || (s_year_time == NULL)) {
		free(_in);
		
		return ret;
	}

	str_trim(s_month);

	uint32_t *p_month = dict_get(__dict_months, s_month);
	
	if(p_month != NULL) {
		ret->month = *p_month;
	}

	bool has_year = strstr(s_year_time, ":") == NULL;

	if(has_year) {
		ret->year = atoi(s_year_time);
	} else {
		//get current year
		time_t t_now;
		struct tm *t_info;
		time(&t_now);
		t_info = localtime(&t_now);
		ret->year = 1900 + t_info->tm_year;

		//get h:s

		char *s_h = strtok_r(s_year_time, ":", &save2);
		char *s_m = strtok_r(NULL, ":", &save2);

		if(s_h != NULL) {
			ret->hour = atoi(s_h);
		}

		if(s_m != NULL) {
			ret->minute = atoi(s_m);
		}

		//stat -la doesnt show year if dir newer than 6 months
		// - in this case we need to manually set year to -1
		if(ret->month > (t_info->tm_mon+1)) {
			ret->year--;
		}
	}

	ret->day = atoi(s_day);

	free(_in);

	return ret;
}

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

struct linked_str_node *parse_xdupe(const char *in) {
	struct linked_str_node *out = NULL;
	char *save, *save2;
	char *_in = strdup(in);
	char *item;

	char *line = strtok_r(_in, "\n", &save);

	while(line != NULL) {

		//strip everything until first 'X' or \0
		while((*line != 'X') && (*line != '\0')) {
			line++;
		}

		//if line[0] = X then continue process if not we can just continue
		if(*line == 'X') {
			//format: 553- X-DUPE: file.bin (after preprocess)=> X-DUPE: file.bin
			str_trim(line);

			item = strtok_r(line, " \t", &save2);

			if((item != NULL) && (strcmp(item, "X-DUPE:") == 0)) {
				item = strtok_r(NULL, " \t", &save2);

				if(item != NULL) {
					str_trim(item);

					if(strlen(item) > 0) {
						struct linked_str_node *node = malloc(sizeof(struct linked_str_node));
						node->str = item;
						node->next = NULL;

						if(out == NULL) {
							out = node;
						} else {
							node->next = out;
							out = node;
						}
					}
				}
			}
		}

		line = strtok_r(NULL, "\n", &save);
	}

	return out;
}
