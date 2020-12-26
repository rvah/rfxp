#include "date.h"

time_t date_to_unixtime(struct date_info *date) {
	time_t t_now;
	struct tm *tmp;
	time(&t_now);
	tmp = localtime(&t_now);

	tmp->tm_year = date->year - 1900;
	tmp->tm_mon = date->month - 1;
	tmp->tm_mday = date->day;
	tmp->tm_hour = date->hour;
	tmp->tm_min = date->minute;
	tmp->tm_sec = 0;

	return mktime(tmp);
}
