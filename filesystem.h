#pragma once

#include "general.h"
#include "util.h"

#define MAX_PATH_LEN 4096
#define MAX_FILENAME_LEN 1024

#define FILE_TYPE_FILE 0
#define FILE_TYPE_DIR 1
#define FILE_TYPE_LINK 2

struct file_item {
	uint32_t file_type;
	char permissions[10];
	char owner_user[255];
	char owner_group[255];
	uint64_t size;
	char date[13];
	char file_name[MAX_FILENAME_LEN];
	struct file_item *next;
};

void print_file_item(struct file_item *item);
struct file_item *parse_list(char *text_list);
struct file_item *parse_line(char *line);
