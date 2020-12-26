#pragma once
#include "general.h"
#include "util.h"

struct hilight_rule {
	char *str;
	struct hilight_rule *next;
};

bool hilight_init(const char *rule_s);
bool hilight_file(const char *s);
