#include "test_ftp.h"
#include "filesystem.h"
#include "ftp.h"
#include "io.h"
#include "mock_filesystem.h"
#include "mock_io.h"
#include "mock_net.h"
#include "transfer_result.h"
#include <bits/stdint-uintn.h>

/*
bool ftp_connect(struct site_info *site);
void ftp_disconnect(struct site_info *site);
bool ftp_retr(struct site_info *site, char *filename);
bool ftp_cwd(struct site_info *site, const char *dirname);
bool ftp_mkd(struct site_info *site, char *dirname);
bool ftp_ls(struct site_info *site);
bool ftp_sscn(struct site_info *site, bool set_on);
bool ftp_prot(struct site_info *site);
bool ftp_pwd(struct site_info *site);
bool ftp_feat(struct site_info *site);
bool ftp_xdupe(struct site_info *site, uint32_t level);
bool ftp_auth(struct site_info *site);
bool ftp_stor(struct site_info *site, char *filename);
bool ftp_cwd_up(struct site_info *site);
struct pasv_details *ftp_pasv(struct site_info *site, bool handshake);
struct transfer_result *ftp_get(struct site_info *site, const char *filename, const char *local_dir, const char *remote_dir);
struct transfer_result *ftp_put(struct site_info *site, const char *filename, const char *local_dir, const char *remote_dir);
struct transfer_result *ftp_get_recursive(struct site_info *site, const char *dirname, const char *local_dir, const char *remote_dir);
struct transfer_result *ftp_put_recursive(struct site_info *site, const char *dirname, const char *local_dir, const char *remote_dir);
*/

static struct site_info *get_site_a(bool secure) {
	return site_init("test1", "192.168.1.1", "7777", "user", "pass", secure);
}

void test_ftp_connect() {
	TEST_IGNORE();
}

void test_ftp_retr() {
	mock_net_reset();
	struct site_info *site = get_site_a(false);

	char *request[] = {
		"RETR file.txt\r\n",
		"RETR badfile.txt\r\n"
	};

	char *response[] = {
		"150 Opening BINARY mode data connection for /file.txt (662 bytes) using SSL/TLS.\n",
		"550 badfile.txt: No such file or directory.\n"
	};


	mock_net_set_socket_response(response);
	mock_net_set_socket_request(request);

	bool succ1 = ftp_retr(site, "file.txt");
	bool fail1 = ftp_retr(site, "");
	bool fail2 = ftp_retr(site, NULL);
	bool fail3 = ftp_retr(site, "badfile.txt");


	TEST_ASSERT_TRUE(succ1);
	TEST_ASSERT_FALSE(fail1);
	TEST_ASSERT_FALSE(fail2);
	TEST_ASSERT_FALSE(fail3);
}

void test_ftp_cwd() {
	struct site_info *site = get_site_a(false);

	mock_net_reset();

	char *request[] = {
		"CWD good_dir\r\n",
		"PWD\r\n",
		"CWD bad_dir\r\n"
	};

	char *response[] = {
		"250- --NEWS--\n"
		"250-\n"
		"250- New Feature:  Login with (!)Username to kill ghost connections.\n"
		"250- New Feature:  Login with (/)Username to disable use_dir_size.\n"
		"250- \n"
		"250- --=- Type SITE HELP for a nice list of SITE commands -=--\n"
		"250-\n"
		"250 CWD command successful.\n",
		"257 \"/good_dir\" is current directory.\n",
		"550 bad_dir: No such file or directory.\n"
	};

	mock_net_set_socket_response(response);
	mock_net_set_socket_request(request);

	bool succ1 = ftp_cwd(site, "good_dir");
	bool cur_cwd1 = strcmp(site->current_working_dir, "/good_dir") == 0;
	bool fail1 = ftp_cwd(site, "");
	bool fail2 = ftp_cwd(site, NULL);
	bool fail3 = ftp_cwd(site, "bad_dir");

	TEST_ASSERT_TRUE(succ1);
	TEST_ASSERT_TRUE(cur_cwd1);
	TEST_ASSERT_FALSE(fail1);
	TEST_ASSERT_FALSE(fail2);
	TEST_ASSERT_FALSE(fail3);
}

void test_ftp_mkd() {
	struct site_info *site = get_site_a(false);

	mock_net_reset();

	char *request[] = {
		"MKD new_dir\r\n",
		"MKD new_dir\r\n"
	};

	char *response[] = {
		"257 \"/new_dir\" created.\n",
		"500 error\n"
	};

	mock_net_set_socket_response(response);
	mock_net_set_socket_request(request);

	bool succ1 = ftp_mkd(site, "new_dir");
	bool fail1 = ftp_mkd(site, "");
	bool fail2 = ftp_mkd(site, NULL);
	bool fail3 = ftp_mkd(site, "newer_dir");

	TEST_ASSERT_TRUE(succ1);
	TEST_ASSERT_FALSE(fail1);
	TEST_ASSERT_FALSE(fail2);
	TEST_ASSERT_FALSE(fail3);
}

void test_ftp_ls() {
	struct site_info *site = get_site_a(false);

	mock_net_reset();

	char *request[] = {
		"STAT -la\r\n",
		"STAT -la\r\n"
	};

	filesystem_set_sort(SORT_TYPE_NAME_ASC);

	char *response[] = {
		"500 bad response\n",
		"213- status of -la:\n"
		"total 81\n"
		"drwsrwxrwx   5 glftpd   glftpd       4096 Dec 23 15:23 .\n"
		"drwxr-xr-x  14 glftpd   glftpd       4096 Nov 13 14:17 ..\n"
		"-rw-r--r--   1 usr      NoGroup      5334 Dec 23 15:23 c.c\n"
		"-rw-r--r--   1 usr      NoGroup     14300 Dec 23 15:23 f.c\n"
		"drwxrwxrwx   3 usr      NoGroup      4096 Dec 23 09:48 home\n"
		"-rw-r--r--   1 usr      NoGroup       662 Dec 23 14:24 t.c\n"
		"drwxrwxrwx   4 usr      NoGroup      4096 Jan 10 07:26 files\n"
		"drwxrwxrwx   3 usr      NoGroup      4096 Dec 26 13:26 txt\n"
		"213 End of Status\n"
	};

	mock_net_set_socket_response(response);
	mock_net_set_socket_request(request);

	bool fail1 = ftp_ls(site);
	bool succ1 = ftp_ls(site);

	TEST_ASSERT_FALSE(fail1);
	TEST_ASSERT_TRUE(succ1);

	TEST_ASSERT_TRUE(strcmp(site->cur_dirlist->file_name, "c.c") == 0);
	site->cur_dirlist = site->cur_dirlist->next;

	TEST_ASSERT_TRUE(strcmp(site->cur_dirlist->file_name, "f.c") == 0);
	site->cur_dirlist = site->cur_dirlist->next;

	TEST_ASSERT_TRUE(strcmp(site->cur_dirlist->file_name, "files") == 0);
	site->cur_dirlist = site->cur_dirlist->next;

	TEST_ASSERT_TRUE(strcmp(site->cur_dirlist->file_name, "home") == 0);
	site->cur_dirlist = site->cur_dirlist->next;

	TEST_ASSERT_TRUE(strcmp(site->cur_dirlist->file_name, "t.c") == 0);
	site->cur_dirlist = site->cur_dirlist->next;

	TEST_ASSERT_TRUE(strcmp(site->cur_dirlist->file_name, "txt") == 0);
	site->cur_dirlist = site->cur_dirlist->next;
}

void test_ftp_sscn() {
	struct site_info *site = get_site_a(false);

	mock_net_reset();

	char *request[] = {
		"SSCN ON\r\n",
		"SSCN OFF\r\n",
		"SSCN ON\r\n",
		"SSCN OFF\r\n"
	};

	char *response[] = {
		"200 SSCN:CLIENT METHOD\n",
		"200 SSCN:CLIENT METHOD\n",
		"500 error\n",
		"500 error\n"
	};

	mock_net_set_socket_response(response);
	mock_net_set_socket_request(request);

	bool succ1 = ftp_sscn(site, !site->sscn_on) && site->sscn_on;
	bool succ2 = ftp_sscn(site, !site->sscn_on) && !site->sscn_on;
	bool fail1 = ftp_sscn(site, !site->sscn_on);
	bool fail2 = ftp_sscn(site, !site->sscn_on);

	TEST_ASSERT_TRUE(succ1);
	TEST_ASSERT_TRUE(succ2);
	TEST_ASSERT_FALSE(fail1);
	TEST_ASSERT_FALSE(fail2);
}

void test_ftp_prot() {
	struct site_info *site = get_site_a(false);

	mock_net_reset();

	char *request[] = {
		"PROT P\r\n",
		"PROT P\r\n"
	};

	char *response[] = {
		"200 Protection set to Private\n",
		"500 Err\n"
	};

	mock_net_set_socket_response(response);
	mock_net_set_socket_request(request);

	TEST_ASSERT_TRUE(ftp_prot(site));
	TEST_ASSERT_FALSE(ftp_prot(site));
}

void test_ftp_pwd() {
	struct site_info *site = get_site_a(false);

	mock_net_reset();

	char *request[] = {
		"PWD\r\n",
		"PWD\r\n"
	};

	char *response[] = {
		"257 \"/good_dir\" is current directory.\n",
		"500 Err\n"
	};

	site_xdupe_add(site, "somefile.zip");

	mock_net_set_socket_response(response);
	mock_net_set_socket_request(request);

	TEST_ASSERT_TRUE(dict_has_key(site->xdupe_table, "somefile.zip"));
	TEST_ASSERT_TRUE(ftp_pwd(site));
	TEST_ASSERT_TRUE(strcmp(site->current_working_dir, "/good_dir") == 0);
	TEST_ASSERT_TRUE(site->xdupe_empty);
	TEST_ASSERT_FALSE(dict_has_key(site->xdupe_table, "somefile.zip"));
	TEST_ASSERT_FALSE(ftp_pwd(site));
}

void test_ftp_feat() {
	struct site_info *site = get_site_a(false);

	mock_net_reset();

	char *request[] = {
		"FEAT\r\n",
		"FEAT\r\n",
		"FEAT\r\n"
	};

	char *response[] = {
		"211- Extensions supported:\n"
		" AUTH TLS\n"
		" AUTH SSL\n"
		" PBSZ\n"
		" PROT\n"
		" CPSV\n"
		" SSCN\n"
		" MDTM\n"
		" SIZE\n"
		" REST STREAM\n"
		" SYST\n"
		" EPRT\n"
		" EPSV\n"
		" CEPR\n"
		"211 End\n",
		"211- Extensions supported:\n"
		" AUTH TLS\n"
		" AUTH SSL\n"
		" PBSZ\n"
		" PROT\n"
		" CPSV\n"
		" MDTM\n"
		" SIZE\n"
		" REST STREAM\n"
		" SYST\n"
		" EPRT\n"
		" EPSV\n"
		" CEPR\n"
		"211 End\n",
		"500 Err\n"
	};

	mock_net_set_socket_response(response);
	mock_net_set_socket_request(request);

	TEST_ASSERT_TRUE(ftp_feat(site));
	TEST_ASSERT_TRUE(site->enable_sscn);
	site->enable_sscn = false;
	TEST_ASSERT_TRUE(ftp_feat(site));
	TEST_ASSERT_FALSE(site->enable_sscn);
	TEST_ASSERT_FALSE(ftp_feat(site));
}

void test_ftp_xdupe() {
	struct site_info *site = get_site_a(false);

	mock_net_reset();

	char *request[] = {
		"SITE XDUPE 3\r\n",
		"SITE XDUPE 3\r\n"
	};

	char *response[] = {
		"200 Activated extended dupe mode 3.\n",
		"500 Err\n"
	};

	mock_net_set_socket_response(response);
	mock_net_set_socket_request(request);

	TEST_ASSERT_TRUE(ftp_xdupe(site, 3));
	TEST_ASSERT_TRUE(site->xdupe_enabled);
	TEST_ASSERT_FALSE(ftp_xdupe(site, 3));
	TEST_ASSERT_FALSE(site->xdupe_enabled);
}

void test_ftp_auth(bool secure) {
	struct site_info *site = get_site_a(secure);
	mock_net_reset();

	struct config *conf = malloc(sizeof(struct config));
	conf->enable_xdupe = true;
	config_set_conf(conf);

	char *request[] = {
		"AUTH TLS\r\n",
		"USER user\r\n",
		"PASS pass\r\n",
		"PBSZ 0\r\n",
		"TYPE I\r\n",
		"PROT P\r\n",
		"FEAT\r\n",
		"SITE XDUPE 3\r\n",
		"STAT -la\r\n",
		"PWD\r\n"
	};

	char *response[] = {
		"234 AUTH TLS successful\n",
		"331 Password required for user.\n",

		"230- Some server motd line1\n"
		"230- Some server motd line2\n"
		"230 User user logged in.\n",

		"200 PBSZ 0 successful\n",
		"200 Type set to I.\n",
		"200 Protection set to Private\n",

		"211- Extensions supported:\n"
		" AUTH TLS\n"
		" AUTH SSL\n"
		" PBSZ\n"
		" PROT\n"
		" CPSV\n"
		" SSCN\n"
		" MDTM\n"
		" SIZE\n"
		" REST STREAM\n"
		" SYST\n"
		" EPRT\n"
		" EPSV\n"
		" CEPR\n"
		"211 End\n",

		"200 Activated extended dupe mode 3.\n",

		"213- status of -la:\n"
		"total 105\n"
		"drwsrwxrwx   8 glftpd   glftpd       4096 Jan 10 10:06 .\n"
		"drwxr-xr-x  14 glftpd   glftpd       4096 Nov 13 14:17 ..\n"
		"drwxrwxrwx   2 user     NoGroup      4096 Jan 10 09:44 some_dir1\n"
		"-rw-r--r--   1 user     NoGroup      5334 Dec 23 15:23 c.c\n"
		"-rw-r--r--   1 user     NoGroup     14300 Dec 23 15:23 f.c\n"
		"drwxrwxrwx   3 user     NoGroup      4096 Dec 23 09:48 home\n"
		"-rw-r--r--   1 user     NoGroup       662 Dec 23 14:24 t.c\n"
		"drwxrwxrwx   4 user     NoGroup      4096 Jan 10 07:26 tv\n"
		"drwxrwxrwx   3 user     NoGroup      4096 Dec 26 13:26 txt\n"
		"213 End of Status\n",

		"257 \"/\" is current directory.\n",
	};

	mock_net_set_socket_response(response);
	mock_net_set_socket_request(request);

	TEST_ASSERT_TRUE(ftp_auth(site));
}

void test_ftp_auth_secure() {
	test_ftp_auth(true);
}

void test_ftp_auth_insecure() {
	TEST_IGNORE();
//	test_ftp_auth(false);
}

void test_ftp_stor() {
	struct site_info *site = get_site_a(false);

	mock_net_reset();

	char *request[] = {
		"STOR /dir/succ.txt\r\n",
		"STOR /dir/fail.txt\r\n",
		"STOR /dir/dupefile1.txt\r\n",
		"STOR /dir/dupefile1.txt\r\n"
	};

	char *response[] = {
		"150 Opening BINARY mode data connection for succ.txt using SSL/TLS.\n",
		"500 Err\n",
		"553- X-DUPE: dupefile1.txt\n"
		"553- X-DUPE: dupefile2.txt\n"
		"553- X-DUPE: dupefile3.txt\n"
		"553- dupefile1.txt: This file looks like a dupe!\n"
		"553 It was uploaded by user ( 0m 37s ago).\n"
	};

	mock_net_set_socket_response(response);
	mock_net_set_socket_request(request);

	TEST_ASSERT_TRUE(ftp_stor(site, "/dir/succ.txt"));
	TEST_ASSERT_FALSE(ftp_stor(site, "/dir/fail.txt"));

	site->xdupe_enabled = true;
	TEST_ASSERT_FALSE(ftp_stor(site, "/dir/dupefile1.txt"));
	TEST_ASSERT_TRUE(dict_has_key(site->xdupe_table, "/dir/dupefile1.txt"));
	TEST_ASSERT_TRUE(dict_has_key(site->xdupe_table, "/dir/dupefile2.txt"));
	TEST_ASSERT_TRUE(dict_has_key(site->xdupe_table, "/dir/dupefile3.txt"));
}

void test_ftp_pasv() {
	struct site_info *site = get_site_a(false);

	mock_net_reset();

	char *request[] = {
		"CPSV\r\n",
		"CPSV\r\n",
		"PASV\r\n",
		"PASV\r\n"
	};

	char *response[] = {
		"227 Entering Passive Mode (127,0,0,1,184,145)\n",
		"227 Entering Passive Mode (127,0,0,1)\n",
		"227 Entering Passive Mode (127,0,0,1,181,107)\n",
		"500 Err\n"
	};

	mock_net_set_socket_response(response);
	mock_net_set_socket_request(request);

	struct pasv_details *succ1 = ftp_pasv(site, true);
	struct pasv_details *fail1 = ftp_pasv(site, true);
	struct pasv_details *succ2 = ftp_pasv(site, false);
	struct pasv_details *fail2 = ftp_pasv(site, false);

	TEST_ASSERT_NOT_NULL(succ1);
	TEST_ASSERT_NULL(fail1);
	TEST_ASSERT_NOT_NULL(succ2);
	TEST_ASSERT_NULL(fail2);

	TEST_ASSERT_TRUE(strcmp(succ1->host, "127.0.0.1") == 0);
	TEST_ASSERT_TRUE(strcmp(succ2->host, "127.0.0.1") == 0);
	TEST_ASSERT_TRUE(succ1->port == 184*256+145);
	TEST_ASSERT_TRUE(succ2->port == 181*256+107);
}

uint8_t *create_random_data(size_t n) {
	uint8_t *d = malloc(sizeof(uint8_t) * n);

	for(size_t i =0; i < n; i++) {
		d[i] = rand() % 0xFF;
	}

	return d;
}

uint8_t *create_empty_data(size_t n) {
	uint8_t *d = malloc(sizeof(uint8_t) * n);

	return d;
}

void test_ftp_get() {
	mock_net_reset();
	mock_filesystem_reset();

	struct site_info *site = get_site_a(false);
	mock_filesystem_set_local_list_buffer(
		"total 8\n"
		"drwxr-xr-x 2 m m 4096 Jan 15 17:09 .\n"
		"drwxrwxrwx 6 m m 4096 Jan 15 17:09 ..\n"
		"-rw-r--r-- 1 m m    0 Jan 15 17:09 one.txt\n"
		"-rw-r--r-- 1 m m    0 Jan 15 17:09 three.bin\n"
		"-rw-r--r-- 1 m m    0 Jan 15 17:09 two.txt\n");

	site->cur_dirlist = filesystem_parse_list(
		"total 8\n"
		"drwxr-xr-x 2 m m 4096 Jan 15 17:09 .\n"
		"drwxrwxrwx 6 m m 4096 Jan 15 17:09 ..\n"
		"-rw-r--r-- 1 m m    0 Jan 15 17:09 one.txt\n"
		"-rw-r--r-- 1 m m    0 Jan 15 17:09 three.bin\n"
		"-rw-r--r-- 1 m m    0 Jan 15 17:09 two.txt\n",
		GLFTPD);

	char *request[] = {
		"PROT P\r\n",
		"PASV\r\n",
		"RETR /three.bin\r\n"
	};

	char *response[] = {
		"200 Protection set to Private\n",
		"227 Entering Passive Mode (127,0,0,1,174,145)\n",
		"150 Opening BINARY mode data connection for /three.bin (5334 bytes) using SSL/TLS.\n",
		"226- [Ul: 737.4MiB] [Dl: 1861.1MiB] [Speed: 786.00KiB/s] [Free: 406159MB]\n"
		"226  [Section: DEFAULT] [Credits: 14.6MiB] [Ratio: UL&DL: Unlimited]\n"
	};

	mock_net_set_socket_response(response);
	mock_net_set_socket_request(request);

	size_t file_n = 1024*1024*5;
	uint8_t *file_src = create_random_data(file_n);
	uint8_t *file_dst = create_empty_data(file_n);

	mock_io_set_ftp_buffer(file_src, file_n);
	mock_io_set_file_buffer(file_dst, file_n);

	//verify skipping of files that exists
	struct transfer_result *fail1 = ftp_get(site, "three.bin", "/", "/");

	mock_filesystem_set_local_list_buffer(
		"total 8\n"
		"drwxr-xr-x 2 m m 4096 Jan 15 17:09 .\n"
		"drwxrwxrwx 6 m m 4096 Jan 15 17:09 ..\n"
		"-rw-r--r-- 1 m m    0 Jan 15 17:09 two.txt\n");

	struct transfer_result *succ1 = ftp_get(site, "three.bin", "/", "/");

	TEST_ASSERT_TRUE(fail1->success && fail1->skipped);
	TEST_ASSERT_TRUE(succ1->success && !succ1->skipped);

	TEST_ASSERT_EQUAL_UINT8_ARRAY(file_src, file_dst, file_n);

	free(file_src);
	free(file_dst);
}

void test_ftp_put() {
	mock_net_reset();
	mock_filesystem_reset();

	struct site_info *site = get_site_a(false);
	mock_filesystem_set_local_list_buffer(
		"total 8\n"
		"drwxr-xr-x 2 m m 4096 Jan 15 17:09 .\n"
		"drwxrwxrwx 6 m m 4096 Jan 15 17:09 ..\n"
		"-rw-r--r-- 1 m m    0 Jan 15 17:09 one.txt\n"
		"-rw-r--r-- 1 m m    0 Jan 15 17:09 three.bin\n"
		"-rw-r--r-- 1 m m    0 Jan 15 17:09 two.txt\n");

	site->cur_dirlist = filesystem_parse_list(
		"total 8\n"
		"drwxr-xr-x 2 m m 4096 Jan 15 17:09 .\n"
		"drwxrwxrwx 6 m m 4096 Jan 15 17:09 ..\n"
		"-rw-r--r-- 1 m m    0 Jan 15 17:09 three.bin\n"
		"-rw-r--r-- 1 m m    0 Jan 15 17:09 two.txt\n",
		GLFTPD);

	char *request[] = {
		"PROT P\r\n",
		"PASV\r\n",
		"STOR /one.txt\r\n"
	};

	char *response[] = {
		"200 Protection set to Private\n",
		"227 Entering Passive Mode (127,0,0,1,174,145)\n",
		"150 Opening BINARY mode data connection for one.txt using SSL/TLS.\n",
		"226- [Ul: 737.4MiB] [Dl: 1861.1MiB] [Speed: 786.00KiB/s] [Free: 406159MB]\n"
		"226  [Section: DEFAULT] [Credits: 14.6MiB] [Ratio: UL&DL: Unlimited]\n"
	};

	mock_net_set_socket_response(response);
	mock_net_set_socket_request(request);

	size_t file_n = 1024*1024*5;
	uint8_t *file_src = create_random_data(file_n);
	uint8_t *file_dst = create_empty_data(file_n);

	mock_io_set_ftp_buffer(file_src, file_n);
	mock_io_set_file_buffer(file_dst, file_n);

	struct transfer_result *succ1 = ftp_put(site, "one.txt", "/" ,"/");

	TEST_ASSERT_TRUE(succ1->success && !succ1->skipped);

	TEST_ASSERT_EQUAL_UINT8_ARRAY(file_src, file_dst, file_n);

	free(file_src);
	free(file_dst);
}

void test_ftp_get_recursive() {
	TEST_IGNORE();
}

void test_ftp_put_recursive() {
	TEST_IGNORE();
}

void test_run_ftp() {
	srand (time(NULL));

	net_set_socket_opener(mock_net_socket_opener);
	net_set_socket_secure_opener(mock_net_socket_secure_opener);
	net_set_socket_closer(mock_net_socket_closer);
	net_set_socket_receiver(mock_net_socket_receiver);
	net_set_socket_sender(mock_net_socket_sender);
	net_set_socket_secure_receiver(mock_net_socket_secure_receiver);
	net_set_socket_secure_sender(mock_net_socket_secure_sender);
	io_set_file_reader(mock_io_file_reader);
	io_set_ftp_reader(mock_io_ftp_reader);
	io_set_file_writer(mock_io_file_writer);
	io_set_ftp_writer(mock_io_ftp_writer);
	filesystem_set_local_lister(mock_filesystem_local_lister);

	RUN_TEST(test_ftp_connect);
	RUN_TEST(test_ftp_retr);
	RUN_TEST(test_ftp_cwd);
	RUN_TEST(test_ftp_mkd);
	RUN_TEST(test_ftp_ls);
	RUN_TEST(test_ftp_sscn);
	RUN_TEST(test_ftp_prot);
	RUN_TEST(test_ftp_pwd);
	RUN_TEST(test_ftp_feat);
	RUN_TEST(test_ftp_xdupe);
	RUN_TEST(test_ftp_auth_secure);
	RUN_TEST(test_ftp_auth_insecure);
	RUN_TEST(test_ftp_stor);
	RUN_TEST(test_ftp_pasv);
	RUN_TEST(test_ftp_get);
	RUN_TEST(test_ftp_put);
	RUN_TEST(test_ftp_get_recursive);
	RUN_TEST(test_ftp_put_recursive);
}
