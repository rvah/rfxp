#pragma once

#include "general.h"
#include "util.h"
#include "skiplist.h"
#include "priolist.h"
#include "sort.h"
#include "hilight.h"

#define MAX_PATH_LEN 4096
#define MAX_FILENAME_LEN 1024

#define FILE_TYPE_FILE 0
#define FILE_TYPE_DIR 1
#define FILE_TYPE_LINK 2

struct file_item {
	struct file_item *next;
	uint32_t file_type;
	char permissions[10];
	char owner_user[255];
	char owner_group[255];
	uint64_t size;
	char date[13];
	char file_name[MAX_FILENAME_LEN];
	bool skip;
	uint32_t priority;
	bool hilight;
};

struct file_item *find_local_file(char *path, char *filename);
struct file_item *local_ls(char *path);
struct file_item *find_file(struct file_item *list, char *filename);
void print_file_item(struct file_item *item);
struct file_item *parse_list(char *text_list);
struct file_item *parse_line(char *line);
