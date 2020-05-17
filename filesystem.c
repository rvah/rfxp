#include "filesystem.h"

static uint32_t __current_sort = SORT_TYPE_TIME_DESC;

uint32_t filesystem_get_sort() {
	return __current_sort;
}

void filesystem_set_sort(uint32_t sort) {
	__current_sort = sort;
}

struct sort_node *az_comparator(void *a, void *b, bool asc) {
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

struct sort_node *time_comparator(void *a, void *b, bool asc) {
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

struct sort_node *size_comparator(void *a, void *b, bool asc) {
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

struct sort_node *general_comparator(void *a, void*b) {
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

struct sort_node *priority_comparator(void *a, void *b) {
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

void dirlist_sort(struct file_item **list, bool prio_sort) {
	if(prio_sort) {
		merge_sort((struct sort_node **)list, priority_comparator);
	} else {
		merge_sort((struct sort_node **)list, general_comparator);
	}
}

void file_item_destroy(struct file_item *item) {
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

struct file_item *file_item_cpy(struct file_item * item) {
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

struct file_item *find_local_file(const char *path, const char *filename) {
	DIR *dir;
	struct dirent *ent;
	struct file_item *item = NULL;

	if ((dir = opendir (path)) != NULL) {
		while ((ent = readdir (dir)) != NULL) {
			if( (strlen(ent->d_name) == 0) ||
				(strcmp(ent->d_name, ".") == 0) || 
				(strcmp(ent->d_name, "..") == 0)) {
				continue;
			}

			if(strcmp(ent->d_name, filename) == 0) {
				if((ent->d_type == DT_DIR) || (ent->d_type == DT_REG)) {
					item = malloc(sizeof(struct file_item));
					strlcpy(item->file_name, ent->d_name, MAX_FILENAME_LEN);
					str_trim(item->file_name);
					item->file_type = (ent->d_type == DT_REG) ? FILE_TYPE_FILE : FILE_TYPE_DIR;
					item->skip = skiplist_skip(item->file_name);
					return item;
				}
			}
		}
		closedir (dir);
	}

	return item;
}

struct file_item *local_ls(char *path, bool prio_sort) {
	DIR *dir;
	struct dirent *ent;
	struct file_item *list = NULL;
	struct file_item *item = NULL;

	if ((dir = opendir (path)) != NULL) {
		while ((ent = readdir (dir)) != NULL) {
			if( (strlen(ent->d_name) == 0) ||
				(strcmp(ent->d_name, ".") == 0) ||
				(strcmp(ent->d_name, "..") == 0)) {
				continue;
			}

			if((ent->d_type == DT_DIR) || (ent->d_type == DT_REG)) {
				item = malloc(sizeof(struct file_item));
				strlcpy(item->file_name, ent->d_name, MAX_FILENAME_LEN);
				item->file_type = (ent->d_type == DT_REG) ? FILE_TYPE_FILE : FILE_TYPE_DIR;
				item->skip = skiplist_skip(item->file_name);
				item->priority = priolist_get_priority(item->file_name);
				item->hilight = hilight_file(item->file_name);
				item->date[0] = '\0';
				item->next = NULL;

				if(list == NULL) {
					list = item;
				} else {
					item->next = list;
					list = item;
				}
			}
		}
	}

	dirlist_sort(&list, prio_sort);

	return list;
}

struct file_item *find_file(struct file_item *list, const char *filename) {
	if(list == NULL) {
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

void print_file_item(struct file_item *item) {
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

struct file_item *parse_list(char *text_list) {
	char *save;
	char *line = strtok_r(text_list, "\n", &save);

	//we want to skip the first totals string, run strtok again
	line = strtok_r(NULL, "\n", &save);
	struct file_item *first_item = NULL;
	struct file_item *prev_item = NULL;

	while(line != NULL) {
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

		struct file_item *item = parse_line(line);

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

struct file_item *parse_line(char *line) {
	if( (line[0] != '-') && (line[0] != 'd') && (line[0] != 'l')) {
		return NULL;
	}

	char *save;
	char *column = strtok_r(line, " \t", &save);

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
