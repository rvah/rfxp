#include "filesystem.h"

static uint32_t __current_sort = SORT_TYPE_TIME_DESC;

/*
 * ----------------
 *
 * Private functions
 *
 * ----------------
 */

static struct sort_node *az_comparator(void *a, void *b, bool asc) {
	if(asc) {
		if(((struct file_item *)a)->file_name[0] <= ((struct file_item *)b)->file_name[0]) {
			return a;
		}
	} else {
		if(((struct file_item *)a)->file_name[0] >= ((struct file_item *)b)->file_name[0]) {
			return a;
		}
	}

	return b;
}

static struct sort_node *time_comparator(void *a, void *b, bool asc) {
	//tmp fix for local dirlist which doesnt have dates
	if(((struct file_item *)a)->date[0] == '\0') {
		return az_comparator(a, b, asc);
	}

	struct date_info *d_a = parse_date(((struct file_item *)a)->date);
	struct date_info *d_b = parse_date(((struct file_item *)b)->date);

	time_t t_a = date_to_unixtime(d_a);
	time_t t_b = date_to_unixtime(d_b);

	free(d_a);
	free(d_b);

	if(asc) {
		if(t_a <= t_b) {
			return a;
		}
	} else {
		if(t_a >= t_b) {
			return a;
		}
	}

	return b;
}

static struct sort_node *size_comparator(void *a, void *b, bool asc) {
	if(asc) {
		if(((struct file_item *)a)->size <= ((struct file_item *)b)->size) {
			return a;
		}
	} else {
		if(((struct file_item *)a)->size >= ((struct file_item *)b)->size) {
			return a;
		}
	}

	return b;
}

static struct sort_node *general_comparator(void *a, void*b) {
	switch(filesystem_get_sort()) {
	case SORT_TYPE_TIME_ASC:
		return time_comparator(a, b, true);
	case SORT_TYPE_TIME_DESC:
		return time_comparator(a, b, false);
	case SORT_TYPE_NAME_ASC:
		return az_comparator(a, b, true);
	case SORT_TYPE_NAME_DESC:
		return az_comparator(a, b, false);
	case SORT_TYPE_SIZE_ASC:
		return size_comparator(a, b, true);
	case SORT_TYPE_SIZE_DESC:
		return size_comparator(a, b, false);
	}

	return a;
}

static struct sort_node *priority_comparator(void *a, void *b) {
	//first check if diff prio
	if(((struct file_item *)a)->priority < ((struct file_item *)b)->priority) {
		return a;
	}

	if(((struct file_item *)a)->priority > ((struct file_item *)b)->priority) {
		return b;
	}

	//if not sort by normal comp
	return general_comparator(a, b);
}

static void dirlist_sort(struct file_item **list, bool prio_sort) {
	if(prio_sort) {
		merge_sort((struct sort_node **)list, priority_comparator);
	} else {
		merge_sort((struct sort_node **)list, general_comparator);
	}
}


struct file_item *parse_line_glftpd(const char *line) {
	if( (line[0] != '-') && (line[0] != 'd') && (line[0] != 'l')) {
		return NULL;
	}

	char *save;
	char *column = strtok_r(strdup(line), " \t", &save);

	struct file_item *item = malloc(sizeof(struct file_item));

	item->next = NULL;

	int i = 0;

	while(column != NULL) {
		char *p;

		switch(i) {
		//filetype+perm
		case 0:
			//filetype
			switch(column[0]) {
			case '-':
				item->file_type = FILE_TYPE_FILE;
				break;
			case 'd':
				item->file_type = FILE_TYPE_DIR;
				break;
			case 'l':
				item->file_type = FILE_TYPE_LINK;
				break;
			}

			//perm
			strlcpy(item->permissions, column+1, 10);
			break;
		//crap
		case 1:
			break;
		//user
		case 2:
			strlcpy(item->owner_user, column, 255);
			break;
		//group
		case 3:
			strlcpy(item->owner_group, column, 255);
			break;
		//size
		case 4:
			item->size = strtol(column, &p, 10);
			break;
		//month
		case 5:
			item->date[0] = '\0';
			strlcat(item->date, column, 13);
			break;
		//day
		case 6:
			strlcat(item->date, " ", 13);
			strlcat(item->date, column, 13);
			break;
		//time/year
		case 7:
			strlcat(item->date, " ", 13);
			strlcat(item->date, column, 13);

			//full name is in saved ptr
			item->file_name[0] = '\0';
			strlcat(item->file_name, save, MAX_FILENAME_LEN);
			str_trim(item->file_name);
			item->skip = skiplist_skip(item->file_name);
			item->priority = priolist_get_priority(item->file_name);
			item->hilight = hilight_file(item->file_name);
			break;
		}

		column = strtok_r(NULL, " \t", &save);
		i++;
	}

	str_trim(item->file_name);

	//skip . and .. dirs, dont want to show those
	if( (strlen(item->file_name) == 0) ||
		(strcmp(item->file_name, ".") == 0) ||
		(strcmp(item->file_name, "..") == 0)) {
		free(item);
		return NULL;
	}

	return item;
}

struct file_item *parse_line_local(const char *line) {
	//for now, using same format as glftpd dirs
	return parse_line_glftpd(line);
}

struct file_item *parse_list(const char *text_list,
		enum filesystem_list_type list_type) {
	char *save;
	char *line = strtok_r(strdup(text_list), "\n", &save);

	struct file_item *first_item = NULL;
	struct file_item *prev_item = NULL;

	while(line != NULL) {
		//if line starts with "total", skip it
		if(strncmp(line, "total", 5) == 0) {
			line = strtok_r(NULL, "\n", &save);
			continue;
		}

		bool foundEnd = false;

		//if line starts with numbers, strip them
		while((*line >= 0x30) && (*line <= 0x39)) {
			line++;

			//if next char is -, read line, if space, we are EOF
			//(proftpd adds code on all lines, glftpd only on last)
			if(*line == '-') {
				line++;
			} else if(*line == ' ') {
				foundEnd = true;
				break;
			}
		}

		if(foundEnd) {
			break;
		}

		struct file_item *item = NULL;
		switch(list_type) {
		case LOCAL:
			item = parse_line_local(line);
			break;
		case GLFTPD:
			item = parse_line_glftpd(line);
			break;
		}

		if(item == NULL) {
			line = strtok_r(NULL, "\n", &save);
			continue;
		}

		if(first_item == NULL) {
			first_item = item;
		} else {
			prev_item->next = item;
		}

		prev_item = item;

		line = strtok_r(NULL, "\n", &save);
	}

	dirlist_sort(&first_item, true);

	return first_item;
}

static char *default_local_lister(const char *path) {
	size_t buf_len = 1024;
	size_t s_tlen = 0;

	char *out = malloc(buf_len);
	out[0] = '\0';

	DIR *dir;
	struct dirent *ent;

	if((dir = opendir(path)) == NULL) {
		goto _filesystem_local_list_err;
	}

	char *fmt = "%crwxrwxrwx 1 %d %d %d %s %s\n";

	while ((ent = readdir (dir)) != NULL) {
		/*
			stat -la format:
			drwxr-xr-x  14 glftpd   glftpd       4096 Nov 13 14:17 ..
			-rw-r--r--   1 user     NoGroup      5334 Dec 23 15:23 c.c
		*/

		char *full_path = path_append_file(path, ent->d_name);
		struct stat s_stat;

		if(lstat(full_path, &s_stat) == -1) {
			printf("error: could not read file: %s\n", full_path);
			free(full_path);
			continue;
		}

		char type = '-';
		uint32_t mode = s_stat.st_mode & S_IFMT;

		switch(mode) {
		case S_IFDIR:
			type = 'd';
			break;
		case S_IFREG:
			type = '-';
			break;
		case S_IFLNK:
			type = 'l';
			break;
		default:
			break;
		}

		char *f_time = time_to_stat_str(s_stat.st_mtime);
		char s_len = snprintf(NULL, 0, fmt, type, s_stat.st_uid, s_stat.st_gid,
				s_stat.st_size, f_time, ent->d_name) + 1;

		char *s_fmt = malloc(s_len);
		snprintf(s_fmt, s_len, fmt, type, s_stat.st_uid, s_stat.st_gid,
				s_stat.st_size, f_time, ent->d_name);

		s_tlen += s_len;

		if(s_tlen > buf_len) {
			buf_len += 1024;
			out = realloc(out, buf_len);
		}

		strlcat(out, s_fmt, buf_len);

		free(s_fmt);
		free(full_path);
		free(f_time);

	}

	return out;

_filesystem_local_list_err:
	free(out);
	return "";
}

static char *(*local_lister)(const char *path) = default_local_lister;

/*
 * ----------------
 *
 * Public functions
 *
 * ----------------
 */

void filesystem_set_local_lister(char *(*lister)(const char *)) {
	local_lister = lister;
}

uint32_t filesystem_get_sort() {
	return __current_sort;
}

void filesystem_set_sort(uint32_t sort) {
	__current_sort = sort;
}

void filesystem_file_item_destroy(struct file_item *item) {
	if(item == NULL) {
		return;
	}

	struct file_item *prev;

	while(item != NULL) {
		prev = item;
		item = item->next;
		free(prev);
	}
}

struct file_item *filesystem_file_item_cpy(struct file_item * item) {
	struct file_item *r_item = NULL;
	struct file_item *first_item = NULL;

	while(item != NULL) {
		if(r_item == NULL) {
			r_item = malloc(sizeof(struct file_item));
			memcpy(r_item, item, sizeof(struct file_item));
			first_item = r_item;
		} else {
			r_item->next = malloc(sizeof(struct file_item));
			memcpy(r_item->next, item, sizeof(struct file_item));
			r_item = r_item->next;
		}
		item = item->next;
	}

	return first_item;
}

struct file_item *filesystem_find_file(struct file_item *list, const char *filename) {
	if(list == NULL) {
		return NULL;
	}

	if(filename == NULL) {
		return NULL;
	}

	while(list != NULL) {
		if(strcmp(list->file_name, filename) == 0) {
			return list;
		}
		list = list->next;
	}

	return NULL;
}

struct file_item *filesystem_filter_list(struct file_item *list, char *file_mask) {
	if(list == NULL) {
		return NULL;
	}

	struct file_item *f = NULL;
	struct file_item *t = NULL;

	while(list != NULL) {
		if(match_rule(file_mask, list->file_name)) {
			t = list->next;

			if(f == NULL) {
				list->next = NULL;
				f = list;
			} else {
				list->next = f;
				f = list;
			}

			list = t;
		} else {
			t = list;
			list = list->next;
			free(t);
		}
	}
	return f;
}


char *filesystem_local_list(const char *path) {
	return local_lister(path);
}

void filesystem_print_file_item(struct file_item *item) {
	printf("--- -  ->\n");
	printf("Type: %d\n", item->file_type);
	printf("Permissions: %s\n", item->permissions);
	printf("Owner/User: %s\n", item->owner_user);
	printf("Owner/Group: %s\n", item->owner_group);
	printf("Size(b): %lu\n", item->size);
	printf("date: %s\n", item->date);
	printf("name: %s\n", item->file_name);
	printf("<-  - ---\n");
}

struct file_item *filesystem_parse_list(const char *text_list,
		enum filesystem_list_type list_type) {
	return parse_list(text_list, list_type);
}
