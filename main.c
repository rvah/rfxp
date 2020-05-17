#include <openssl/evp.h>
#include "general.h"
#include "conn.h"
#include "crypto.h"
#include "filesystem.h"
#include "site.h"
#include "ui.h"
#include "config.h"
#include "msg.h"
#include "log.h"
#include "skiplist.h"

void show_logo() {
	printf(TCOL_RED "                                   .------.                                         \n" TCOL_RESET);
	printf(TCOL_RED "        __  ____   _   __________>>>.MFXP.<<<__________   _   ____  __              \n" TCOL_RESET);
	printf(TCOL_RED "        \\//.\\  // / \\\\ \\  _ ___  \\__  __  __/  ___ _  / // \\ \\\\  /.\\\\/   \n" TCOL_RESET);
	printf(TCOL_RED "       .______// /   \\\\ \\     /   \\\\\\ \\/ ///   \\     / //   \\ \\\\_______. \n" TCOL_RESET);
	printf(TCOL_RED "       |        /________\\___/   .\\\\\\\\/\\////.   \\___/________\\         |    \n" TCOL_RESET);
	printf(TCOL_RED "       :  .______.___________________   _______.     _____________     :            \n" TCOL_RESET);
	printf(TCOL_RED "         _|      |       \\__    ____/___\\      |_____\\__________  \\_   .        \n" TCOL_RESET);
	printf(TCOL_RED "       . \\_    _   /    . | .  ________/       /     _|  .     /   /\\             \n" TCOL_RESET);
	printf(TCOL_RED "       .  |     \\_/       | .  \\      |\\_    _/      \\_  .  ______/  \\ .       \n" TCOL_RESET);
	printf(TCOL_RED "       :  |      |      : | ::. \\     | /    \\       . | :.      |\\  / .         \n" TCOL_RESET);
	printf(TCOL_RED "       |  |______|    .:: l______\\    |/______\\  . .:: l____\\    | \\/  :        \n" TCOL_RESET);
	printf(TCOL_RED "       :  .\\      \\_______|\\      \\___l \\      \\_______|\\    \\___| |:  .    \n" TCOL_RESET);
	printf(TCOL_RED "       ..:::\\______\\      \\ \\______\\   \\ \\______\\      \\ \\____\\  \\ |::.:\n" TCOL_RESET);
	printf(TCOL_RED "       . <==========\\______\\/=======\\___\\/=======\\______\\/=====\\__\\|=> .    \n" TCOL_RESET);
	printf(TCOL_RED "       .   _ __ ___ ___ _______        _        _______ ____ ___ __ _  .            \n" TCOL_RESET);
	printf(TCOL_RED "       |____________  / \\     //    .// \\\\.    \\\\     / \\  ____________|      \n" TCOL_RESET);
	printf(TCOL_RED "                 bHe\\//. \\   ///.  __/   \\__  .\\\\\\   / .\\\\/sE!              \n" TCOL_RESET);
	printf(TCOL_RED "                          \\_______/         \\_______/                             \n" TCOL_RESET);
	printf(TCOL_GREEN "       Version %s\n\n" TCOL_RESET, MFXP_VERSION);

}

void init() {
	log_init();
	msg_init();
	ssl_init();

	char *conf_path = expand_home_path("~/.mfxp/config.ini");
	
	if(!config_read(conf_path)) {
		printf("failed to read config!\n");
		exit(1);
	}

	//start ident thread
	pthread_t ident_thread;
	pthread_create(&ident_thread, NULL, thread_ident, NULL);
}

void destroy() {
	ssl_cleanup();
	config_cleanup();
	log_cleanup();
}

void get_pw() {
	char *pw = getpass("enter encryption key:");

	if(strlen(pw) < 8) {
		printf("encryption key has to be at least 8 chars long!\n");
		exit(1);
	}

	unsigned int salt[] = {46124, 78314};

	if(!aes_init((unsigned char *)pw, strlen(pw), (unsigned char *)&salt)) {
		printf("Could not init the AES cipher\n");
		exit(1);
	}
}

int32_t main( int32_t argc, char **argv ) {
	show_logo();

	get_pw();	

	init();
	ui_init();
	ui_loop();
	ui_close();
	destroy();

	return 0;
}
