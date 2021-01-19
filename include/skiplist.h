#pragma once
#include "general.h"
#include "util.h"
#include "log.h"

struct skiplist_rule {
	char *str;
	struct skiplist_rule *next;
};

void skiplist_print();
bool skiplist_skip(char *s);
bool skiplist_init(const char *rule_s);
