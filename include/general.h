#pragma once
#include <bsd/string.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <libgen.h>
#include <wordexp.h>
#include <sys/time.h>

#define MFXP_VERSION "0.4"

#define NFO_DL_MAX_SZ 102400

struct linked_str_node {
	char *str;
	struct linked_str_node *next;
};
