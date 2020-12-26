#pragma once
#include "general.h"
#include "site.h"
#include "stats.h"
#include "parse.h"
#include "colors.h"
char *generate_ui_prompt(char indicator_l, char indicator_r);
char *generate_ui_dirlist_item(const char *color, struct file_item *file);
