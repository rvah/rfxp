#include "ftp.h"
#include "filesystem.h"
#include "io.h"
#include "net.h"
#include "stats.h"

/*
 * ----------------
 * Private functions
 * ----------------
 */

bool valid_str_arg(const char *str) {
	return str != NULL && strlen(str) > 0;
}

bool open_data(struct site_info *site) {
	if(!site->prot_sent && !ftp_prot(site)) {
		return false;
	}

	struct pasv_details *pv = ftp_pasv(site, false);

	if(pv == NULL) {
		return false;
	}

	char port[8];
	sprintf(port, "%d", pv->port);

	site->data_socket_fd = net_open_socket(pv->host, port);

	if(site->data_socket_fd == -1) {
		return false;
	}

	return true;
}

bool data_enable_tls(struct site_info *site) {
	SSL_CTX *ctx = ssl_get_context();

	if(ctx == NULL) {
		log_w("failed to get SSL context\n");
		net_close_socket(site->data_socket_fd);
		return false;
	}

	SSL *ssl = SSL_new(ctx);
	SSL_set_fd(ssl, site->data_socket_fd);

	if(net_open_socket_secure(ssl) == -1) {
		log_w("TLS FAILED, ERROR:\n");
		ERR_print_errors_fp(stderr);
		SSL_CTX_free(ctx);
		net_close_socket(site->data_socket_fd);
		return false;
	}

	site->data_secure_fd = ssl;

	return true;
}


struct transfer_result *get_recursive(struct site_info *site,
		const char *dirname, const char *local_dir, const char *remote_dir) {
	struct transfer_result *ret_val = NULL;
	struct file_item *first_file = NULL;
	char *new_lpath = NULL;
	char *new_rpath = NULL;

	//make copy of dirname,local_dir,remote_dir to prevent cwd from manip. ptr
	char *_dirname = strdup(dirname);
	char *_local_dir = strdup(local_dir);
	char *_remote_dir = strdup(remote_dir);

	struct file_item *file = filesystem_find_file(site->cur_dirlist, _dirname);

	if(file == NULL) {
		ret_val = transfer_result_create(
				false, strdup(_dirname), 0, 0.0f, false, FILE_TYPE_DIR);
		goto _get_recursive_cleanup;
	}

	if(file->skip) {
		log_ui_w("%s: skip\n", _dirname);
		ret_val = transfer_result_create(
				true, strdup(_dirname), 0, 0.0f, true, FILE_TYPE_DIR);
		goto _get_recursive_cleanup;
	}

	new_lpath = path_append_dir(_local_dir, _dirname);
	new_rpath = path_append_dir(_remote_dir, _dirname);

	if(!ftp_cwd(site, new_rpath)) {
		ret_val = transfer_result_create(
				false, strdup(_dirname), 0, 0.0f, false, FILE_TYPE_DIR);
		goto _get_recursive_cleanup;
	}

	if(!file_exists(new_lpath)) {
		log_ui_i("%s: creating dir\n", new_lpath);
		if(mkdir(new_lpath, 0755) != 0) {
			log_ui_e("%s: unable to mkdir!\n", new_lpath, FILE_TYPE_DIR);
			ret_val = transfer_result_create(
					false, strdup(_dirname), 0, 0.0f, false, FILE_TYPE_DIR);
			goto _get_recursive_cleanup;
		}
	}

	if(!ftp_ls(site)) {
		log_ui_e("%s: unable to get dirlist!\n", _dirname);
		ret_val = transfer_result_create(
				false, strdup(dirname), 0, 0.0f, false, FILE_TYPE_DIR);
		goto _get_recursive_cleanup;
	}

	if(site->cur_dirlist == NULL) {
		log_ui_w("%s: empty dir\n", _dirname);
		ret_val = transfer_result_create(
				true, strdup(_dirname), 0, 0.0f, true, FILE_TYPE_DIR);
		goto _get_recursive_cleanup;
	}

	struct file_item *fl = site->cur_dirlist;

	ret_val = transfer_result_create(
			true, strdup(_dirname), 0, 0.0f, false, FILE_TYPE_DIR);
	struct transfer_result *rp = ret_val;

	//count files in list
	uint32_t tot_files = 0;
	uint32_t cur_file_num = 0;

	while(fl != NULL) {
		if(fl->file_type == FILE_TYPE_FILE) {
			tot_files++;
		}
		fl = fl->next;
	}

	//make a copy, to prevent subsequent cwd from freeing the list
	first_file = filesystem_file_item_cpy(site->cur_dirlist);
	fl = first_file;

	while(fl != NULL) {
		//move to end of ret list
		while(rp->next != NULL) {
			rp = rp->next;
		}

		if(fl->file_type == FILE_TYPE_FILE) {
			cur_file_num++;

			log_ui_i("%s: downloading file [%d/%d]\n", fl->file_name,
					cur_file_num, tot_files);

			struct transfer_result *f_ret = ftp_get(
					site, fl->file_name, new_lpath, new_rpath);

			if(!f_ret->success) {
				log_ui_e("%s: download failed!\n", fl->file_name);
			}

			//append stat
			rp->next = f_ret;

		} else if(fl->file_type == FILE_TYPE_DIR) {
			log_ui_i("%s: downloading dir..\n", fl->file_name);

			rp->next = get_recursive(site, fl->file_name, new_lpath, new_rpath);

			//restore current path
			if(!ftp_cwd(site, new_rpath)) {
				//mark dir as failed
				ret_val->success = false;
				break;
			}

			if(!ftp_ls(site)) {
				ret_val->success = false;
				break;
			}
		}

		fl = fl->next;
	}

_get_recursive_cleanup:
	filesystem_file_item_destroy(first_file);

	free(new_lpath);
	free(new_rpath);

	free(_local_dir);
	free(_remote_dir);
	free(_dirname);
	return ret_val;
}

struct transfer_result *put_recursive(struct site_info *site,
		const char *dirname, const char *local_dir, const char *remote_dir) {
	struct transfer_result *ret_val = NULL;

	char *new_lpath = NULL;
	char *new_rpath = NULL;
	struct file_item *l_list = NULL;

	//cpy args to prevent outside changes during cwd etc if ptr from dirlist etc
	char *_dirname = strdup(dirname);
	char *_local_dir = strdup(local_dir);
	char *_remote_dir = strdup(remote_dir);

	char *str_list = filesystem_local_list(_local_dir);
	struct file_item *f_files = filesystem_parse_list(str_list, LOCAL);
	struct file_item *local_file = filesystem_find_file(f_files, _dirname);

	free(str_list);

	if(local_file == NULL) {
		ret_val = transfer_result_create(
				false, strdup(_dirname), 0, 0.0f, false, FILE_TYPE_DIR);
		goto _put_recursive_cleanup;
	}

	if(local_file->skip) {
		log_ui_w("%s: skip\n", _dirname);
		ret_val = transfer_result_create(
				true, strdup(_dirname), 0, 0.0f, true, FILE_TYPE_DIR);
		goto _put_recursive_cleanup;
	}

	new_lpath = path_append_dir(_local_dir, _dirname);
	new_rpath = path_append_dir(_remote_dir, _dirname);
	struct file_item *rfile = filesystem_find_file(site->cur_dirlist, _dirname);

	if(rfile == NULL) {
		if(!ftp_mkd(site, new_rpath)) {
			log_ui_e("%s: mkdir failed!\n", _dirname);
		}
	} else if(rfile->file_type != FILE_TYPE_DIR) {
		log_ui_e("%s: remote file not a directory!\n", _dirname);
		ret_val = transfer_result_create(
				false, strdup(_dirname), 0, 0.0f, false, FILE_TYPE_DIR);
		goto _put_recursive_cleanup;
	}

	if(!ftp_cwd(site, new_rpath)) {
		ret_val = transfer_result_create(
				false, strdup(_dirname), 0, 0.0f, false, FILE_TYPE_DIR);
		goto _put_recursive_cleanup;
	}

	if(!ftp_ls(site)) {
		ret_val = transfer_result_create(
				false, strdup(_dirname), 0, 0.0f, false, FILE_TYPE_DIR);
		goto _put_recursive_cleanup;
	}

	//TODO: check prio
	char *s_list = filesystem_local_list(new_lpath);
	l_list = filesystem_parse_list(s_list, LOCAL);
	free(s_list);
	struct file_item *lp = l_list;

	if(lp == NULL) {
		log_ui_w("%s: empty dir\n", _dirname);
		ret_val = transfer_result_create(
				true, strdup(_dirname), 0, 0.0f, true, FILE_TYPE_DIR);
		goto _put_recursive_cleanup;
	}

	ret_val = transfer_result_create(
			true, strdup(_dirname), 0, 0.0f, false, FILE_TYPE_DIR);
	struct transfer_result *rp = ret_val;

	//count files in list
	uint32_t tot_files = 0;
	uint32_t cur_file_num = 0;

	while(lp != NULL) {
		if(lp->file_type == FILE_TYPE_FILE) {
			tot_files++;
		}
		lp = lp->next;
	}

	lp = l_list;

	while(lp != NULL) {
		//move to end of ret list
		while(rp->next != NULL) {
			rp = rp->next;
		}

		if(lp->file_type == FILE_TYPE_FILE) {
			cur_file_num++;
			log_ui_i("%s: uploading file [%d/%d]\n", lp->file_name,
					cur_file_num, tot_files);

			struct transfer_result *f_ret = ftp_put(
					site, lp->file_name, new_lpath, new_rpath);

			if(!f_ret->success) {
				log_ui_e("%s: upload failed!\n", lp->file_name);
			}

			//append stat
			rp->next = f_ret;
		} else if(lp->file_type == FILE_TYPE_DIR) {
			log_ui_i("%s: uploading dir..\n", lp->file_name);

			rp->next = put_recursive(site, lp->file_name, new_lpath, new_rpath);

			//restore current path
			if(!ftp_cwd(site, new_rpath)) {
				//mark dir as failed
				ret_val->success = false;
				break;
			}

			if(!ftp_ls(site)) {
				ret_val->success = false;
				break;
			}

		}

		lp = lp->next;
	}

_put_recursive_cleanup:
	free(local_file);
	free(new_lpath);
	free(new_rpath);

	free(_local_dir);
	free(_remote_dir);
	free(_dirname);

	//free dir data
	while(l_list != NULL) {
		lp = l_list;
		l_list = l_list->next;
		free(lp);
	}

	return ret_val;
}

/*
 * ----------------
 * Public functions
 * ----------------
 */

bool ftp_connect(struct site_info *site) {
	site->is_connecting = true;

	site_busy(site);
	int32_t sockfd = net_open_socket(site->address, site->port);

	if(sockfd == -1) {
		site_idle(site);
		site->is_connecting = false;
		return false;
	}

	site->socket_fd = sockfd;

	uint32_t code = control_recv(site);

	if(code != 220) {
		ftp_disconnect(site);
		site->is_connecting = false;
		return false;
	}

	//proftpd seems troublesome with sscn
	//if proftpd identified, always set SSCN ON(client mode) on the other server
	//TODO: figure out if proftpd issue, or if its just my test server
	if(strstr(site->last_recv, "ProFTPD") != NULL) {
		site->enforce_sscn_server_mode = true;
	}

	//do auth
	bool ret = ftp_auth(site);

	site->is_connecting = false;
	return ret;
}

void ftp_disconnect(struct site_info *site) {
	net_close_socket(site->socket_fd);
	log_w("FTP session was terminated\n");
}

bool ftp_retr(struct site_info *site, char *filename) {
	if(!valid_str_arg(filename)) {
		return false;
	}

	int slen = strlen(filename)+9;
	char *s_retr = malloc(slen);

	snprintf(s_retr, slen, "RETR %s\r\n", filename);
	bool ret = control_send(site, s_retr) && control_recv(site) == 150;

	free(s_retr);

	return ret;
}


bool ftp_cwd(struct site_info *site, const char *dirname) {
	if(!valid_str_arg(dirname)) {
		return false;
	}

	int cmd_len = strlen(dirname) + 7;
	char *s_cmd = malloc(cmd_len);

	snprintf(s_cmd, cmd_len, "CWD %s\r\n", dirname);

	bool ret = control_send(site, s_cmd)
		&& control_recv(site) == 250 && ftp_pwd(site);

	free(s_cmd);

	return ret;
}

bool ftp_mkd(struct site_info *site, char *dirname) {
	if(!valid_str_arg(dirname)) {
		return false;
	}

	int cmd_len = strlen(dirname) + 7;
	char *s_cmd = malloc(cmd_len);

	snprintf(s_cmd, cmd_len, "MKD %s\r\n", dirname);

	bool ret = control_send(site, s_cmd) && control_recv(site) == 257;
	free(s_cmd);

	return ret;
}

bool ftp_ls(struct site_info *site) {
	//get list
	if(!control_send(site, "STAT -la\r\n")) {
		return false;
	}

	uint32_t code = control_recv(site);

	if((code != 213) && (code != 212)) {
		return false;
	}

	struct file_item *fl = filesystem_parse_list(site->last_recv, GLFTPD);
	filesystem_file_item_destroy(site->cur_dirlist);
	site->cur_dirlist = fl;

	return true;
}


bool ftp_sscn(struct site_info *site, bool set_on) {
	if(site->sscn_on == set_on) {
		return true;
	}

	char *s_on = "SSCN ON\r\n";
	char *s_off = "SSCN OFF\r\n";

	char *s_cmd = set_on ? s_on : s_off;

	if(control_send(site, s_cmd) && control_recv(site) == 200) {
		site->sscn_on = set_on;
		return true;
	}

	return false;
}

bool ftp_prot(struct site_info *site) {
	if(control_send(site, "PROT P\r\n") && control_recv(site) == 200) {
		site->prot_sent = true;
		return true;
	}

	return false;
}

bool ftp_pwd(struct site_info *site) {
	//read pwd
	if(!control_send(site, "PWD\r\n")) {
		return false;
	}

	uint32_t code = control_recv(site);

	if(code != 257) {
		return false;
	}

	char *pwd = parse_pwd(site->last_recv);

	if(pwd == NULL) {
		log_w("error parsing pwd\n");
		return false;
	}

	site_set_cwd(site, pwd);

	free(pwd);
	log_w("pwd set to %s\n", site->current_working_dir);

	//always clear xdupe on cwd, to prevent annoying cache
	if(!site->xdupe_empty) {
		site_xdupe_clear(site);
	}

	return true;
}


bool ftp_feat(struct site_info *site) {
	if(!control_send(site, "FEAT\r\n")) {
		return false;
	}

	//check features supported that we might use
	uint32_t code = control_recv(site);

	if(code != 211) {
		return false;
	}

	struct linked_str_node *list = parse_feat(site->last_recv);
	struct linked_str_node *prev;

	while(list != NULL) {
		if(strcmp(list->str, "SSCN") == 0) {
			site->enable_sscn = true;
		}

		prev = list;
		list = list->next;

		free(prev);
	}

	return true;
}

bool ftp_xdupe(struct site_info *site, uint32_t level) {
	char s[16];

	snprintf(s, 16, "SITE XDUPE %d\r\n", level);

	bool ret = control_send(site, s) && control_recv(site) == 200;

	site->xdupe_enabled = ret;

	return ret;
}

bool ftp_auth(struct site_info *site) {
	int32_t code;
	char s_auth[SITE_USER_MAX+8];
	char s_pass[SITE_PASS_MAX+8];

	//init TLS connection if enabled
	if(site->use_tls) {
		if(!control_send(site, "AUTH TLS\r\n")) {
			return false;
		}

		code = control_recv(site);

		if(code != 234) {
			log_w("auth tls failed(code %d)\n", code);
			return false;
		}

		SSL_CTX *ctx = ssl_get_context();

		if(ctx == NULL) {
			log_w("failed to get SSL context\n");
			return false;
		}


		SSL *ssl = SSL_new(ctx);
		SSL_set_fd(ssl, site->socket_fd);

		if(net_open_socket_secure(ssl) == -1) {
			log_w("TLS FAILED, ERROR:\n");
			ERR_print_errors_fp(stderr);
			SSL_CTX_free(ctx);
			return false;
		}

		site->secure_fd = ssl;
		site->is_secure = true;
	}

	//send username
	snprintf(s_auth, SITE_USER_MAX+8, "USER %s\r\n", site->username);

	if(!control_send(site, s_auth)) {
		return false;
	}

	code = control_recv(site);

	//username failed?
	if(code != 331) {
		ftp_disconnect(site);
		return false;
	}

	//send password
	snprintf(s_pass, SITE_PASS_MAX+8, "PASS %s\r\n", site->password);

	if(!control_send(site, s_pass)) {
		return false;
	}

	code = control_recv(site);

	//password fail?
	if(code != 230) {
		ftp_disconnect(site);
		return false;
	}

	//set protection buffer to 0
	if(!control_send(site, "PBSZ 0\r\n")) {
		return false;
	}

	code = control_recv(site);

	//set binary mode
	if(!control_send(site, "TYPE I\r\n")) {
		return false;
	}

	code = control_recv(site);

	//set protection P
	if(!ftp_prot(site)) {
		return false;
	}

	//get features
	if(!ftp_feat(site)) {
		return false;
	}

	//enable xdupe if set in conf
	if(config_get_conf()->enable_xdupe) {
		if(!ftp_xdupe(site, 3)) {
			return false;
		}
	}

	//get initial filelist && ret
	return ftp_ls(site) && ftp_pwd(site);
}



struct pasv_details *ftp_pasv(struct site_info *site, bool handshake) {
	bool sent = false;

	if(handshake) {
		sent = control_send(site, "CPSV\r\n");
	} else {
		sent = control_send(site, "PASV\r\n");
	}
	
	if(!sent) {
		return NULL;
	}

	if(control_recv(site) != 227) {
		return NULL;
	}

	struct pasv_details *pv = malloc(sizeof(struct pasv_details));

	if(!parse_pasv(site->last_recv, pv->host, &pv->port, pv->unparsed)) {
		return NULL;
	}

	return pv;
}


bool ftp_stor(struct site_info *site, char *filename) {
	if(!valid_str_arg(filename)) {
		return false;
	}

	int slen = strlen(filename)+9;
	char *s_retr = malloc(slen);

	snprintf(s_retr, slen, "STOR %s\r\n", filename);

	bool ret = control_send(site, s_retr) && control_recv(site) == 150;

	free(s_retr);

	//if xdupe enabled and there are xdupe strings in output, parse it
	if(site->xdupe_enabled && (strstr(site->last_recv, "X-DUPE") != NULL)) {
		struct linked_str_node *xdupe_list = parse_xdupe(site->last_recv);
		struct linked_str_node *tmp_node;
		char *p_dir = path_get_dir_path(filename);

		while(xdupe_list != NULL) {
			char *path = path_append_file(p_dir, xdupe_list->str);
			site_xdupe_add(site, path);

			tmp_node = xdupe_list;
			xdupe_list = xdupe_list->next;

			//free data
			free(path);
			free(tmp_node);
		}

		free(p_dir);
	}

	return ret;
}

bool ftp_cwd_up(struct site_info *site) {
	return ftp_cwd(site, "..") && ftp_ls(site);
}

struct transfer_result *ftp_get(struct site_info *site, const char *filename,
		const char *local_dir, const char *remote_dir) {
	struct transfer_result *ret_val = NULL;
	char *new_lpath = NULL;
	char *new_rpath = NULL;
	struct file_item *loc_file = NULL;

	struct io_item *src = NULL;
	struct io_item *dst = NULL;
	struct stats_transfer *stats = NULL; 

	struct file_item *file = filesystem_find_file(site->cur_dirlist, filename);

	if(file == NULL) {
		ret_val = transfer_result_create(false, strdup(filename), 0, 0.0f,
				false, FILE_TYPE_FILE);
		goto _ftp_get_cleanup;
	}

	char *s_list = filesystem_local_list(local_dir);
	struct file_item *f_files = filesystem_parse_list(s_list, LOCAL);
	loc_file = filesystem_find_file(f_files, filename);

	free(s_list);

	if(loc_file != NULL) {
		log_ui_w("%s: file exists, skipping\n", filename);
		ret_val = transfer_result_create(true, strdup(filename), 0, 0.0f, true,
				FILE_TYPE_FILE);
		goto _ftp_get_cleanup;
	}

	if(file->skip) {
		log_ui_w("%s: skip\n", filename);
		ret_val = transfer_result_create(true, strdup(filename), 0, 0.0f, true,
				FILE_TYPE_FILE);
		goto _ftp_get_cleanup;
	}

	//make sure sscn is off
	if(site->enable_sscn && !ftp_sscn(site, false)) {
		ret_val = transfer_result_create(false, strdup(filename), 0, 0.0f,
				false, FILE_TYPE_FILE);
		goto _ftp_get_cleanup;
	}


	if(!open_data(site)) {
		ret_val = transfer_result_create(false, strdup(filename), 0, 0.0f,
				false, FILE_TYPE_FILE);
		goto _ftp_get_cleanup;
	}

	new_lpath = path_append_file(local_dir, filename);
	new_rpath = path_append_file(remote_dir, filename);

	if(!ftp_retr(site, new_rpath)) {
		net_close_socket(site->data_socket_fd);
		ret_val = transfer_result_create(false, strdup(filename), 0, 0.0f,
				false, FILE_TYPE_FILE);
		goto _ftp_get_cleanup;
	}

	site_busy(site);

	if(site->use_tls && !data_enable_tls(site)) {
		ret_val = transfer_result_create(false, strdup(filename), 0, 0.0f,
				false, FILE_TYPE_FILE);
		goto _ftp_get_cleanup;
	}

	src = io_item_create_remote(site);
	dst = io_item_create_local(new_lpath);
	stats = stats_transfer_create();
	ssize_t tot_read = 0;

	if((tot_read = io_transfer_data(src, dst,
			(void (*)(void *, size_t))stats_transfer_update, stats)) == -1) {	
		ret_val = transfer_result_create(false, strdup(filename), 0, 0.0f,
				false, FILE_TYPE_FILE);
		goto _ftp_get_cleanup;
	} else {
		stats->status = sts_DONE;
		stats_transfer_update(stats, 0);
	}

	if(control_recv(site) != 226) {
		ret_val = transfer_result_create(false, strdup(filename), 0, 0.0f,
				false, FILE_TYPE_FILE);
		goto _ftp_get_cleanup;
	}

	//calculate speed on succ. transfer
	char *speed = s_get_speed(stats->tot_avg_speed);
	char *s_size = parse_file_size(tot_read);

	log_ui_i("%s: downloaded %s at %s in %.2fs\n", filename, s_size, speed,
			stats_transfer_duration(stats));

	free(s_size);
	free(speed);
	ret_val = transfer_result_create(true, strdup(filename), tot_read,
			stats->tot_avg_speed, false, FILE_TYPE_FILE);

_ftp_get_cleanup:
	site->current_speed = 0.0;
	io_item_destroy(src);
	io_item_destroy(dst);
	stats_transfer_destroy(stats);
	free(new_lpath);
	free(new_rpath);

	free(loc_file);
	return ret_val;
}

struct transfer_result *ftp_put(struct site_info *site, const char *filename,
		const char *local_dir, const char *remote_dir) {
	struct transfer_result *ret_val = NULL;
	char *new_lpath = NULL;
	char *new_rpath = NULL;

	struct io_item *src = NULL;
	struct io_item *dst = NULL;
	struct stats_transfer *stats = NULL;

	char *s_list = filesystem_local_list(local_dir);
	struct file_item *f_files = filesystem_parse_list(s_list, LOCAL);

	struct file_item *local_file = filesystem_find_file(f_files, filename);
	free(s_list);

	if(local_file == NULL) {
		ret_val = transfer_result_create(false, strdup(filename), 0, 0.0f,
				false, FILE_TYPE_FILE);
		goto _ftp_put_cleanup;
	}

	if(local_file->skip) {
		log_ui_w("%s: skip\n", filename);
		ret_val = transfer_result_create(true, strdup(filename), 0, 0.0f, true,
				FILE_TYPE_FILE);
		goto _ftp_put_cleanup;
	}

	new_rpath = path_append_file(remote_dir, filename);

	if(site->xdupe_enabled && site_xdupe_has(site, new_rpath)) {
		log_ui_w("%s: skip (x-dupe)\n", filename);
		ret_val = transfer_result_create(true, strdup(filename), 0, 0.0f, true,
				FILE_TYPE_FILE);
		goto _ftp_put_cleanup;
	}

	//make sure sscn is off
	if(site->enable_sscn && !ftp_sscn(site, false)) {
		ret_val = transfer_result_create(false, strdup(filename), 0, 0.0f,
				false, FILE_TYPE_FILE);
		goto _ftp_put_cleanup;
	}

	if(!open_data(site)) {
		ret_val = transfer_result_create(false, strdup(filename), 0, 0.0f,
				false, FILE_TYPE_FILE);
		goto _ftp_put_cleanup;
	}

	new_lpath = path_append_file(local_dir, filename);

	if(!ftp_stor(site, new_rpath)) {
		net_close_socket(site->data_socket_fd);
		ret_val = transfer_result_create(false, strdup(filename), 0, 0.0f,
				false, FILE_TYPE_FILE);
		goto _ftp_put_cleanup;
	}

	site_busy(site);

	if(site->use_tls && !data_enable_tls(site)) {
		ret_val = transfer_result_create(false, strdup(filename), 0, 0.0f,
				false, FILE_TYPE_FILE);
		goto _ftp_put_cleanup;
	}

	src = io_item_create_local(new_lpath);
	dst = io_item_create_remote(site);
	stats = stats_transfer_create();
	ssize_t tot_sent = 0;

	if((tot_sent = io_transfer_data(src, dst,
			(void (*)(void *, size_t))stats_transfer_update, stats)) == -1) {
		ret_val = transfer_result_create(false, strdup(filename), 0, 0.0f,
				false, FILE_TYPE_FILE);
		goto _ftp_put_cleanup;
	} else {
		//do final update of stats(to make sure we get a speed for small files)
		stats->status = sts_DONE;
		stats_transfer_update(stats, 0);
	}

	if(control_recv(site) != 226) {
		ret_val = transfer_result_create(false, strdup(filename), 0, 0.0f,
				false, FILE_TYPE_FILE);
		goto _ftp_put_cleanup;
	}

	//calculate speed on succ. transfer
	char *s_speed = s_get_speed(stats->tot_avg_speed);
	char *s_size = parse_file_size(tot_sent);
	log_ui_i("%s: uploaded %s at %s in %.2fs\n", filename, s_size, s_speed,
			stats_transfer_duration(stats));

	free(s_size);
	free(s_speed);
	ret_val = transfer_result_create(true, strdup(filename), tot_sent,
			stats->tot_avg_speed, false, FILE_TYPE_FILE);

_ftp_put_cleanup:
	site->current_speed = 0.0;
	io_item_destroy(src);
	io_item_destroy(dst);
	stats_transfer_destroy(stats);
	free(new_lpath);
	free(new_rpath);
	free(local_file);
	return ret_val;
}


struct transfer_result *ftp_get_recursive(struct site_info *site,
		const char *dirname, const char *local_dir, const char *remote_dir) {
	struct timeval time_start;
	struct timeval time_end;

	gettimeofday(&time_start, NULL);

	char *remote_origin = strdup(remote_dir);
	struct transfer_result *ret = get_recursive(
			site, dirname, local_dir, remote_dir);

	gettimeofday(&time_end, NULL);

	ftp_cwd(site, remote_origin);
	ftp_ls(site);

	log_ui_i("Stats: %s\n",
			s_gen_stats(ret, time_end.tv_sec - time_start.tv_sec));

	free(remote_origin);

	return ret;
}


struct transfer_result *ftp_put_recursive(struct site_info *site,
		const char *dirname, const char *local_dir, const char *remote_dir) {
	struct timeval time_start;
	struct timeval time_end;

	gettimeofday(&time_start, NULL);

	char *remote_origin = strdup(remote_dir);
	struct transfer_result *ret = put_recursive(
			site, dirname, local_dir, remote_dir);

	gettimeofday(&time_end, NULL);

	ftp_cwd(site, remote_origin);
	ftp_ls(site);

	log_ui_i( "Stats: %s\n",
			s_gen_stats(ret, time_end.tv_sec - time_start.tv_sec));

	free(remote_origin);

	return ret;
}
