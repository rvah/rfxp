#pragma once
#include "general.h"
#include "util.h"

struct priolist_rule {
	char *str;
	struct priolist_rule *next;
};

bool priolist_init(const char *rule_s);
uint32_t priolist_get_priority(const char *s);
