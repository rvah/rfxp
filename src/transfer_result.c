#include "transfer_result.h"

struct transfer_result *transfer_result_create(bool succ, char *filename, uint64_t size, double speed, bool skipped, uint32_t file_type) {
	struct transfer_result *r = malloc(sizeof(struct transfer_result));

	r->next = NULL;
	r->success = succ;
	r->n_transferred = 0;
	r->filename = strdup(filename);
	r->size = size;
	r->speed = speed;
	r->skipped = skipped;
	r->file_type = file_type;

	return r;
}

void transfer_result_destroy(struct transfer_result *result) {
	struct transfer_result *prev;

	while(result != NULL) {
		prev = result;
		result = result->next;

		free(prev->filename);
		free(prev);
	}
}

bool transfer_succ(struct transfer_result *result) {
	struct transfer_result *p = result;
	bool succ = true;

	while(p != NULL) {
		if(!p->success) {
			succ = false;
			break;
		}

		p = p->next;
	}

	transfer_result_destroy(result);
	return succ;
}
