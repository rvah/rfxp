#pragma once
#include "general.h"

struct transfer_result {
	struct transfer_result *next;
	bool success;
	uint32_t n_transferred;
	uint32_t n_failed;
	char *filename;
	uint64_t size;
	double speed;
	bool skipped;
	uint32_t file_type;
};

struct transfer_result *transfer_result_create(bool succ, char *filename, uint64_t size, double speed, bool skipped, uint32_t file_type);
void transfer_result_destroy(struct transfer_result *result);
bool transfer_succ(struct transfer_result *result);
