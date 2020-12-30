#include "hilight.h"

static struct hilight_rule *hilight_rules = NULL;

bool hilight_init(const char *rule_s) {
	if(rule_s == NULL) {
		return false;
	}

	char *save;
	char *s = strtok_r(strdup(rule_s), " \r", &save);

	while(s != NULL) {
		struct hilight_rule *nr = malloc(sizeof(struct hilight_rule));
		nr->str = s;
		nr->next = NULL;

		if(hilight_rules == NULL) {
			hilight_rules = nr;
		} else {
			nr->next = hilight_rules;
			hilight_rules = nr;
		}

		s = strtok_r(NULL, " \r", &save);
	}


	return true;
}

bool hilight_file(const char *s) {
	struct hilight_rule *p = hilight_rules;

	while(p != NULL) {
		if(match_rule(p->str, s)) {
			return true;
		}
		p = p->next;
	}

	return false;
}
