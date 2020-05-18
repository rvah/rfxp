#pragma once
#include "general.h"
#include "time.h"

#define INT_MONTH_JAN 1;
#define INT_MONTH_FEB 2;
#define INT_MONTH_MAR 3;
#define INT_MONTH_APR 4;
#define INT_MONTH_MAY 5;
#define INT_MONTH_JUN 6;
#define INT_MONTH_JUL 7;
#define INT_MONTH_AUG 8;
#define INT_MONTH_SEP 9;
#define INT_MONTH_OCT 10;
#define INT_MONTH_NOV 11;
#define INT_MONTH_DEC 12;

struct date_info {
	uint32_t year;
	uint32_t month;
	uint32_t day;
	uint32_t hour;
	uint32_t minute;
};

time_t date_to_unixtime(struct date_info *date);
