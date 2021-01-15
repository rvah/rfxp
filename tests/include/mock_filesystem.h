#pragma once

#include <stdint.h>
#include <openssl/ssl.h>
#include "general.h"
#include "filesystem.h"

void mock_filesystem_set_local_list_buffer(char *s);
char *mock_filesystem_local_lister(const char *path);
bool mock_filesystem_local_lister_verify_opened(uint32_t times);
void mock_filesystem_reset();
