#pragma once
#include "general.h"

//prime number
#define DICT_SIZE 10007

struct dict_node {
	struct dict_node *next;
	uint32_t key;
	void *value;
};

uint32_t dict_hash(const char *key);
bool dict_has_key(struct dict_node **dict, const char *key);
void *dict_get(struct dict_node **dict, const char *key);
void dict_set(struct dict_node **dict, const char *key, void *value);
void dict_clear(struct dict_node **dict, bool free_value);
void dict_destroy(struct dict_node **dict, bool free_value);
struct dict_node **dict_create();
