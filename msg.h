#pragma once
#include <pthread.h>
#include "general.h"

#define THREAD_ID_UI 0

#define EV_SITE_LS 1
#define EV_SITE_CWD 2
#define EV_SITE_CLOSE 3
#define EV_UI_LOG 101
#define EV_UI_RM_SITE 102

struct msg {
	uint32_t to_id;
	uint32_t from_id;
	uint32_t event;
	void *data;
	struct msg *next;
	struct msg *last;
};

bool msg_init();
void msg_print_q();
void msg_send(struct msg *msg);
struct msg *msg_read(uint32_t id);
