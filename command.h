#pragma once

#include "general.h"
#include "site.h"
#include "conn.h"
#include "config.h"
#include "msg.h"
#include "thread.h"

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
