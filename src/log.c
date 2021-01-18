#include "log.h"

static FILE *logfd = NULL;
static pthread_mutex_t log_mtx;

bool log_init() {
	if(pthread_mutex_init(&log_mtx, NULL) != 0) {
		return false;
	}

	logfd = fopen("mfxp.log", "a+");
	return logfd != NULL;
}

void log_cleanup() {
	fclose(logfd);
}

void log_print(uint32_t n) {
	pthread_mutex_lock(&log_mtx);

	//count lines
	rewind(logfd);

	uint64_t max_n = 0;
	uint64_t cur_line = 0;
	char c;

	while((c = fgetc(logfd)) != EOF) {
		if(c == '\n') {
			max_n++;
		}
	}

	//loop through again and print n last lines
	fseek(logfd, 0L, SEEK_SET);

	while((c = fgetc(logfd)) != EOF) {
		if( (max_n - cur_line) <= n) {
			printf("%c", c);
		}

		if(c == '\n') {
			cur_line++;
		}
	}

	fseek(logfd, 0L, SEEK_END);

	pthread_mutex_unlock(&log_mtx);
}

void log_w(char *format, ...) {
	if(logfd == NULL) {
		return;
	}

	pthread_mutex_lock(&log_mtx);	

	va_list args;
	va_start(args, format);
	vfprintf(logfd, format, args);
	va_end(args);
	fflush(logfd);

	pthread_mutex_unlock(&log_mtx);
}


void log_ui(uint32_t from_id, uint32_t type, char *format, ...) {
	va_list args;
	va_start(args, format);
	int s_len = vsnprintf(NULL, 0, format, args)+2;
	va_end(args);

	char *s_data = malloc(s_len);

	va_start(args, format);
	vsnprintf(s_data, s_len, format, args);
	va_end(args);

	struct msg *m = malloc(sizeof(struct msg));
	struct ui_log *data = malloc(sizeof(struct ui_log));

	data->str = s_data;
	data->type = type;

	m->to_id = THREAD_ID_UI;
	m->from_id = from_id;
	m->event = EV_UI_LOG;
	m->data = (void *)data;
	msg_send(m);
}

void log_ui_e(char *format, ...) {
	va_list args;
	va_start(args, format);
	log_ui(-1, LOG_T_E,	format, args);
	va_end(args);
}

void log_ui_w(char *format, ...) {
	va_list args;
	va_start(args, format);
	log_ui(-1, LOG_T_W,	format, args);
	va_end(args);
}

void log_ui_i(char *format, ...) {
	va_list args;
	va_start(args, format);
	log_ui(-1, LOG_T_I,	format, args);
	va_end(args);
}

void log_ui_d(char *format, ...) {
	va_list args;
	va_start(args, format);
	log_ui(-1, LOG_T_D,	format, args);
	va_end(args);
}
