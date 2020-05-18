#pragma once
#include "general.h"

struct sort_node {
	struct sort_node *next;
};

void merge_sort(struct sort_node **head_ref, struct sort_node *(*comparator)(void *a, void *b));
