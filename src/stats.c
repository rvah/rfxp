#include "stats.h"

uint64_t time_to_usec(struct timeval *t) {
	return t->tv_sec * 1000000 + t->tv_usec;
}

double calc_transfer_speed(struct timeval *start, struct timeval *end, uint64_t size) {
	uint64_t usec_start = time_to_usec(start);
	uint64_t usec_end = time_to_usec(end);
	uint64_t diff_usec = usec_end - usec_start;

	double bps = (double)size / diff_usec;

	return bps * 1000000.0f;
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

struct stats_transfer *stats_transfer_create() {
	struct stats_transfer *r = malloc(sizeof(struct stats_transfer));

	r->status = sts_PROGRESS;
	gettimeofday(&r->t_start, NULL);
	gettimeofday(&r->t_end, NULL);
	r->tot_avg_speed = 0.0;
	r->cur_avg_speed = 0.0;
	r->tot_bytes_sent = 0;

	return r;
}

void stats_transfer_update(struct stats_transfer *s, size_t bytes_sent) {
	s->tot_bytes_sent += bytes_sent;

	struct timeval t_cur;
	gettimeofday(&t_cur, NULL);

	if(time_to_usec(&t_cur) - time_to_usec(&s->t_end) >= STATS_UPDATE_FREQ_USEC
			|| s->status == sts_DONE) {
		s->tot_avg_speed = calc_transfer_speed(&s->t_start, &t_cur, s->tot_bytes_sent);
		s->cur_avg_speed = calc_transfer_speed(&s->t_end, &t_cur, bytes_sent);
	}

	s->t_end = t_cur;
}

double stats_transfer_duration(struct stats_transfer *s) {
	return (double)(time_to_usec(&s->t_end) - time_to_usec(&s->t_start)) / 1000000;
}

void stats_transfer_destroy(struct stats_transfer *stats) {
	free(stats);
}
