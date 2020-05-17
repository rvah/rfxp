#pragma once

#include "general.h"
#include "site.h"
#include "conn.h"
#include "config.h"
#include "msg.h"
#include "thread.h"
#include "filesystem.h"

void cmd_execute(uint32_t thread_id, uint32_t event, void *data);

void cmd_help(char *line);
void cmd_open(char *line, char which);
void cmd_close(char *line, char which);

void cmd_ls(char *line, char which);
void cmd_ref(char *line, char which);
void cmd_cd(char *line, char which);
void cmd_put(char *line, char which);
void cmd_get(char *line, char which);
void cmd_rm(char *line, char which);
void cmd_site(char *line, char which);
void cmd_quote(char *line, char which);
void cmd_fxp(char *line, char which);
void cmd_mkdir(char *line, char which);
void cmd_quit(char *line, char which);
void cmd_log(char *line, char which);
void cmd_nfo(char *line, char which);

void cmd_local_ls(char *line);
void cmd_local_cd(char *line);
void cmd_local_rm(char *line);
void cmd_local_mkdir(char *line);

void cmd_set_sort(char *line, char type);

void cmd_sm(char *line);
