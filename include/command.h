#pragma once

#include "general.h"
#include "site.h"
#include "control.h"
#include "config.h"
#include "msg.h"
#include "thread.h"
#include "filesystem.h"
#include "queue.h"

void cmd_execute(uint32_t thread_id, uint32_t event, void *data);

void cmd_help(char *line);

void cmd_ls(char *line, char which);
void cmd_ref(char *line, char which);
void cmd_cd(char *line, char which);
void cmd_put(char *line, char which);
void cmd_get(char *line, char which);
void cmd_rm(char *line, char which);
void cmd_site(char *line, char which);
void cmd_quote(char *line, char which);
void cmd_open(char *line, char which);
void cmd_close(char *line, char which);
void cmd_fxp(char *line, char which);
void cmd_mkdir(char *line, char which);
void cmd_nfo(char *line, char which);
void cmd_qput(char *line, char which);
void cmd_qget(char *line, char which);
void cmd_qfxp(char *line, char which);

void cmd_quit(char *line);
void cmd_log(char *line);

void cmd_local_ls(char *line);
void cmd_local_cd(char *line);
void cmd_local_rm(char *line);
void cmd_local_mkdir(char *line);

void cmd_sort(char *line, char type);
void cmd_nsort(char *line);
void cmd_tsort(char *line);
void cmd_ssort(char *line);

void cmd_sm(char *line);

void cmd_qx(char *line);
void cmd_qls(char *line);
void cmd_qrm(char *line);

void cmd_lls(char *line);
void cmd_rls(char *line);
void cmd_lref(char *line);
void cmd_rref(char *line);
void cmd_lcd(char *line);
void cmd_rcd(char *line);
void cmd_lput(char *line);
void cmd_rput(char *line);
void cmd_lget(char *line);
void cmd_rget(char *line);
void cmd_lrm(char *line);
void cmd_rrm(char *line);
void cmd_lsite(char *line);
void cmd_rsite(char *line);
void cmd_lquote(char *line);
void cmd_rquote(char *line);
void cmd_lopen(char *line);
void cmd_ropen(char *line);
void cmd_lclose(char *line);
void cmd_rclose(char *line);
void cmd_lfxp(char *line);
void cmd_rfxp(char *line);
void cmd_lmkdir(char *line);
void cmd_rmkdir(char *line);
void cmd_lnfo(char *line);
void cmd_rnfo(char *line);
void cmd_lqput(char *line);
void cmd_rqput(char *line);
void cmd_lqget(char *line);
void cmd_rqget(char *line);
void cmd_lqfxp(char *line);
void cmd_rqfxp(char *line);
