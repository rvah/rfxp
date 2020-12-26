#include "stats.h"

double calc_transfer_speed(struct timeval *start, struct timeval *end, uint64_t size) {
	uint64_t msec_start = start->tv_sec * 1000000 + start->tv_usec; 
	uint64_t msec_end = end->tv_sec * 1000000 + end->tv_usec;
	uint64_t diff_msec = msec_end - msec_start;

	double diff_sec = (double)diff_msec / 1000000.0f;
	double bps = (double)size / diff_sec;

	return bps;
}

char *s_get_speed(double speed) {
	char *out = malloc(20);
	char *unit = malloc(5);

	snprintf(unit, 5, "B/s");

	if(speed > 1024.0) {
		snprintf(unit, 5, "KB/s");
		speed /= 1024.0;

		if(speed > 1024.0) {
			snprintf(unit, 5, "MB/s");
			speed /= 1024.0;

			if(speed > 1024.0) {
				snprintf(unit, 5, "GB/s");
				speed /= 1024.0;
			}
		}
	}

	snprintf(out, 20, "%.2f%s", speed, unit);
	free(unit);

	return out;
}

char *s_calc_transfer_speed(struct timeval *start, struct timeval *end, uint64_t size) {
	return s_get_speed(calc_transfer_speed(start, end, size));
}

char *s_gen_stats(struct transfer_result *tr, uint64_t seconds) {
	struct transfer_result *p = tr;
	uint32_t n_files_succ = 0;
	uint32_t n_files_fail = 0;
	uint32_t n_files_skip = 0;

	uint32_t n_dirs_succ = 0;
	uint32_t n_dirs_fail = 0;
	uint32_t n_dirs_skip = 0;

	uint64_t tot_size = 0;
	double tot_speed = 0.0f;
	double avg_speed = 0.0f;

	char *s_stat = malloc(1024);

	while(p != NULL) {
		if(p->file_type == FILE_TYPE_FILE) {
			if(p->skipped) {
				n_files_skip++;
				p = p->next;
				continue;
			}

			if(p->success) {
				n_files_succ++;
				tot_size += p->size;
				tot_speed += p->speed;
			} else {
				n_files_fail++;
			}
		} else if(p->file_type == FILE_TYPE_DIR) {
			if(p->skipped) {
				n_dirs_skip++;
				p = p->next;
				continue;
			}

			if(p->success) {
				n_dirs_succ++;
			} else {
				n_dirs_fail++;
			}
		}
		p = p->next;
	}

	if(n_files_succ == 0) {
		avg_speed = 0.0f;
	} else {
		avg_speed = tot_speed / n_files_succ;
	}

	char *s_size = parse_file_size(tot_size);
	char *s_speed = s_get_speed(avg_speed);

	snprintf(s_stat, 1024, "%d files, %d dirs totaling %s, transferred at avg %s in %lus. (Skipped: %d files, %d dirs. Failed: %d files, %d dirs)", n_files_succ, n_dirs_succ, s_size, s_speed, seconds, n_files_skip, n_dirs_skip, n_files_fail, n_dirs_fail);

	free(s_speed);
	free(s_size);

	return s_stat;	
}
