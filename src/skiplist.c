#include "skiplist.h"

struct skiplist_rule *skiplist_rules = NULL;

void skiplist_print() {
	struct skiplist_rule *p = skiplist_rules;

	while(p != NULL) {
		printf("%s\n", p->str);
		p = p->next;
	}
}

bool skiplist_skip(char *s) {
	struct skiplist_rule *p = skiplist_rules;

	while(p != NULL) {
		if(match_rule(p->str, s)) {
			return true;
		}
		p = p->next;
	}

	return false;
}

bool skiplist_init(const char *rule_s) {
	if(rule_s == NULL) {
		return false;
	}

	char *save;

	char *s = strtok_r(strdup(rule_s), " \r", &save);


	while(s != NULL) {
		struct skiplist_rule *nr = malloc(sizeof(struct skiplist_rule));
		nr->str = s;
		nr->next = NULL;

		if(skiplist_rules == NULL) {
			skiplist_rules = nr;
		} else {
			nr->next = skiplist_rules;
			skiplist_rules = nr;
		}

		s = strtok_r(NULL, " \r", &save);
	}

	return true;
}
