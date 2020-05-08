#pragma once
#include "general.h"

bool match_rule(const char *rule, const char *str);
void str_ltrim(char *s);
void str_trim(char *s);
void str_rtrim_slash(char *s);
bool file_exists (char *filename);
char *str_get_path(char *s);
char *str_get_file(char *s);
char *path_append_file(char *path, char *file);
char *path_append_dir(char *path, char *dir);
double calc_transfer_speed(struct timeval *start, struct timeval *end, uint64_t size);
char *s_get_speed(double speed);
char *s_calc_transfer_speed(struct timeval *start, struct timeval *end, uint64_t size);
