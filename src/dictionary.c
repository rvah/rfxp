#include "dictionary.h"

uint32_t dict_hash(const char *key) {
	uint32_t hv = 0;

	while(*key != '\0') {
		hv = (hv << 4) + *(key++);

		uint32_t g = hv & 0xF0000000;

		if(g != 0) {
			hv ^= g >> 24;
		}

		hv &= ~g;
	}

	return hv;
}

uint32_t dict_get_index(uint32_t key_hash) {
	return key_hash % DICT_SIZE;
}

bool dict_has_key(struct dict_node **dict, const char *key) {
	uint32_t k_hash = dict_hash(key);
	uint32_t index = dict_get_index(k_hash);

	struct dict_node *p = dict[index];

	while(p != NULL) {
		if(p->key == k_hash) {
			return true;
		}

		p = p->next;
	}

	return false;
}

void *dict_get(struct dict_node **dict, const char *key) {
	uint32_t k_hash = dict_hash(key);
	uint32_t index = dict_get_index(k_hash);

	struct dict_node *p = dict[index];

	while(p != NULL) {
		if(p->key == k_hash) {
			return p->value;
		}

		p = p->next;
	}

	return NULL;
}

void dict_set(struct dict_node **dict, const char *key, void *value) {
	uint32_t k_hash = dict_hash(key);
	uint32_t index = dict_get_index(k_hash);

	//if index is free, or if key is same, straight insert/replace
	if( (dict[index] == NULL) || (dict[index]->key == k_hash) ) {
		//free if already set
		if(dict[index] != NULL) {
			free(dict[index]);
		}

		//insert
		dict[index] = malloc(sizeof(struct dict_node));

		dict[index]->next = NULL;
		dict[index]->key = k_hash;
		dict[index]->value = value;

		return;
	}

	//if not, add to node list
	struct dict_node *p = dict[index];

	while(p->next != NULL) {
		p = p->next;
	}

	p->next = malloc(sizeof(struct dict_node));

	p->next->next = NULL;
	p->next->key = k_hash;
	p->next->value = value;
}

void dict_clear(struct dict_node **dict, bool free_value) {
	for(int i = 0; i < DICT_SIZE; i++) {
		if(dict[i] != NULL) {
			if(free_value) {
				free(dict[i]->value);
			}

			free(dict[i]);
			dict[i] = NULL;
		}
	}
}

void dict_destroy(struct dict_node **dict, bool free_value) {
	dict_clear(dict, free_value);
	free(dict);
}

struct dict_node **dict_create() {
	struct dict_node **dict = malloc(DICT_SIZE * sizeof(struct dict_node));

	for(int i = 0; i < DICT_SIZE; i++) {
		dict[i] = NULL;
	}

	return dict;
}
