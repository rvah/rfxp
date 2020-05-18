#pragma once
#include "general.h"
#include <ctype.h>
#include "transfer_result.h"
#include "filesystem.h"

char *expand_home_path(const char *in);
bool match_rule(const char *rule, const char *str);
void str_ltrim(char *s);
void str_rtrim(char *s);
void str_rtrim_special_only(char *s);
void str_trim(char *s);
void str_rtrim_slash(char *s);
bool file_exists (char *filename);
char *str_get_path(char *s);
char *str_get_file(char *s);
char *path_append_file(const char *path, const char *file);
char *path_append_dir(const char *path, const char *dir);
void str_tolower(char *s);
