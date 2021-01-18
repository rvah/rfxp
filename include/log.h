#pragma once
#include "general.h"
#include "msg.h"

#define LOG_T_E 0
#define LOG_T_W 1
#define LOG_T_I 2
#define LOG_T_D 3
#define LOG_UI_MAX_LEN 2048

struct ui_log {
	char *str;
	uint32_t type;
};

bool log_init();
void log_cleanup();
void log_print(uint32_t n);
void log_w(char *format, ...);
void log_ui(uint32_t from_id, uint32_t type, char *format, ...);
void log_ui_e(char *format, ...);
void log_ui_w(char *format, ...);
void log_ui_i(char *format, ...);
void log_ui_d(char *format, ...);
