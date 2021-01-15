#include "mock_filesystem.h"

static uint32_t local_lister_call_count = 0;
static char *local_list_buf = NULL;

void mock_filesystem_set_local_list_buffer(char *s) {
	local_list_buf = s;
}

char *mock_filesystem_local_lister(const char *path) {
	return strdup(local_list_buf);
}

bool mock_filesystem_local_lister_verify_opened(uint32_t times) {
	return times == local_lister_call_count;
}

void mock_filesystem_reset() {
	local_lister_call_count = 0;
	local_list_buf = NULL;
}
