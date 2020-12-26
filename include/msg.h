#pragma once
#include <pthread.h>
#include "general.h"

#define THREAD_ID_UI 0
#define THREAD_ID_IDENT 1
#define THREAD_ID_QUEUE 2

#define EV_SITE_LS 1
#define EV_SITE_CWD 2
#define EV_SITE_CLOSE 3
#define EV_SITE_GET 4
#define EV_SITE_PUT 5
#define EV_SITE_FXP 6
#define EV_SITE_RM 7
#define EV_SITE_SITE 8
#define EV_SITE_QUOTE 9
#define EV_SITE_MKDIR 10
#define EV_SITE_VIEW_NFO 11

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
bool msg_q_empty();
void msg_print_q();
void msg_send(struct msg *msg);
struct msg *msg_read(uint32_t id);
