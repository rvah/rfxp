#pragma once
#include "general.h"
#include "net.h"
#include "log.h"
#include "util.h"
#include "site.h"

#define IDENT_MAX_PORT_LEN 6
#define IDENT_MAX_NAME_LEN 255

struct ident_config {
	bool enabled;
	bool always_on;
	char port[IDENT_MAX_PORT_LEN];
	char name[IDENT_MAX_NAME_LEN];
};

void ident_set_setting(const char *name, const char *value);
void ident_start();
