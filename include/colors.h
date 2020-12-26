#pragma once
#include "general.h"
#include "util.h"

#define TCOL_RED "\x1B[31m"
#define TCOL_GREEN "\x1B[32m"
#define TCOL_BLUE "\x1B[34m"
#define TCOL_PINK "\x1B[35m"
#define TCOL_WHITE "\x1B[37m"
#define TCOL_CYAN "\x1B[36m"
#define TCOL_YELLOW "\x1B[33m"
#define TCOL_RESET "\x1B[0m"

static const char * const __color_red = "\x1B[31m";
static const char * const __color_green = "\x1B[32m";
static const char * const __color_blue = "\x1B[34m";
static const char * const __color_pink = "\x1B[35m";
static const char * const __color_white = "\x1B[37m";
static const char * const __color_cyan = "\x1B[36m";
static const char * const __color_yellow = "\x1B[33m";

struct color_config {
	const char *skip_file;
	const char *hilight_file;
	const char *file_type_file;
	const char *file_type_dir;
	const char *file_type_symlink;
};

struct color_config *colors_get_conf();
void colors_set_setting(const char *name, const char *value);
