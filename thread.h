#pragma once
#include <pthread.h>
#include <readline/readline.h>
#include "general.h"
#include "site.h"
#include "conn.h"
#include "msg.h"
#include "log.h"
#include "colors.h"
#include "filesystem.h"

void *thread_ui(void *ptr);
void *thread_site(void *ptr);
