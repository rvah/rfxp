#pragma once
#include "general.h"
#include <ctype.h>
#include "transfer_result.h"
#include "filesystem.h"
#include "parse.h"

double calc_transfer_speed(struct timeval *start, struct timeval *end, uint64_t size);
char *s_get_speed(double speed);
char *s_calc_transfer_speed(struct timeval *start, struct timeval *end, uint64_t size);
char *s_gen_stats(struct transfer_result *tr, uint64_t seconds);
