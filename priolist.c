#include "priolist.h"

struct priolist_rule *priolist_rules = NULL;

bool priolist_init(const char *rule_s) {
    if(rule_s == NULL) {
        return false;
    }

    char *save;
    char *s = strtok_r(strdup(rule_s), " \r", &save);

	struct priolist_rule *first;

    while(s != NULL) {
        struct priolist_rule *nr = malloc(sizeof(struct priolist_rule));
        nr->str = s;
        nr->next = NULL;

        if(priolist_rules == NULL) {
            priolist_rules = nr;
			first = nr;
        } else {
            priolist_rules->next = nr;
			priolist_rules = priolist_rules->next;
        }

        s = strtok_r(NULL, " \r", &save);
    }

	//rewind
	priolist_rules = first;

    return true;
}

uint32_t priolist_get_priority(const char *s) {
	struct priolist_rule *p = priolist_rules;
	uint32_t priority = 0;

	while(p != NULL) {
		if(match_rule(p->str, s)) {
			return priority;
		}

		priority++;
		p = p->next;
	}

	return priority;
}
