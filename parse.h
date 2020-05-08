#pragma once
#include "general.h"
#include "util.h"

bool parse_pasv(const char *in, char *out_addr, uint32_t *out_port, char *out_unparsed);
char *parse_pwd(const char *s);
char *parse_file_size(uint64_t size);
struct linked_str_node *parse_feat(const char *in);
