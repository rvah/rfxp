#include "filesystem.h"

struct file_item *find_file(struct file_item *list, char *filename) {
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
			break;
		//name
		//case 8:
//			item->file_name[0] = '\0';
//			strlcat(item->file_name, column, MAX_FILENAME_LEN);
//			printf("savept: %s\n", save);
//			break;
		//default:
			//add everything else to filename..
			//strlcat(item->file_name, " ", MAX_FILENAME_LEN);
			//strlcat(item->file_name, column, MAX_FILENAME_LEN);
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
