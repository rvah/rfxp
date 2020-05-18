#include "conn.h"

uint32_t read_code(char *buf) {
	char a = buf[0] - 0x30;
	char b = buf[1] - 0x30;
	char c = buf[2] - 0x30;

	//check if numbers between 0 to 9, if not return with error
	if(((a > 9) || (a < 0)) || ((b > 9) || (b < 0)) || ((c > 9) || (c < 0))) {
		return -1;
	}

	return a*100 + b*10 + c;
}

bool control_end(char *buf) {
	return buf[3] == ' ';
}

ssize_t read_data_socket(struct site_info *site, void *buf, size_t len, bool force_plaintext) {
	if(site->use_tls && !force_plaintext) {
		return SSL_read(site->data_secure_fd, buf, len);
	} else {
		return recv(site->data_socket_fd, buf, len, 0);
	}
}

ssize_t write_data_socket(struct site_info *site, const void *buf, size_t len, bool force_plaintext) {
	if(site->use_tls && !force_plaintext) {
		return SSL_write(site->data_secure_fd, buf, len);
	} else {
		return send(site->data_socket_fd, buf, len, 0);
	}
}

ssize_t read_control_socket(struct site_info *site, void *buf, size_t len, bool force_plaintext) {
	if(site->use_tls && !force_plaintext) {
		return SSL_read(site->secure_fd, buf, len);
	} else {
		return recv(site->socket_fd, buf, len, 0);
	}
}

ssize_t write_control_socket(struct site_info *site, const void *buf, size_t len, bool force_plaintext) {
	if(site->use_tls && !force_plaintext) {
		return SSL_write(site->secure_fd, buf, len);
	} else {
		return send(site->socket_fd, buf, len, 0);
	}
}

bool __control_send(struct site_info *site, char *data, bool force_plaintext) {
	site_busy(site);

	uint32_t len = strlen(data);
	uint32_t n_sent = 0;
	uint32_t n = 0;

	//censor logging off PASS cmd
	if(strncmp(data, "PASS", 4) == 0) {
		log_w("PASS <<CENSORED>>\n");
	} else {
		log_w("%s",data);
	}

	while(n_sent < len) {
		n = write_control_socket(site, data+n_sent, len-n_sent, force_plaintext);
		n_sent += n;

		if(n == -1) {
			site_idle(site);
			return false;
		}
	}

	site_idle(site);
	return true;
}

int32_t __control_recv(struct site_info *site, bool force_plaintext) {
	site_busy(site);
	char buf[CONTROL_BUF_SZ];
	char line[CONTROL_LINE_SZ]; // control code + space/dash

	uint32_t code, col_count = 0;
	int32_t numbytes;
	int32_t out_sz = CONTROL_INT_BUF_CHUNK_SZ;
	int32_t out_cur_ofs = 0;

	if(site->last_recv != NULL) {
		//free old received data
		free(site->last_recv);
		site->last_recv = NULL;
	}

	//initial size 4k
	site->last_recv = malloc(out_sz);

	if(site->last_recv == NULL) {
		site_idle(site);
		return -1;
	}

	site->last_recv[0] = '\0';

	while((numbytes = read_control_socket(site, buf, CONTROL_BUF_SZ-1, force_plaintext)) != 0) {
		if (numbytes == -1) {
			log_w("error recv\n");
			site_idle(site);
			return -1;
		}

		for(int i = 0; i < numbytes; i++) {
			line[col_count] = buf[i];

			if( (out_cur_ofs+2) > out_sz) {
				out_sz += CONTROL_INT_BUF_CHUNK_SZ;
				site->last_recv = realloc(site->last_recv, out_sz);
			}

			site->last_recv[out_cur_ofs] = buf[i];
			out_cur_ofs++;

			//TODO: improve this whole thing
			if(buf[i] == '\n') {
				//write line
				line[col_count+1] = '\0';
				log_w("%s",line);

				//check if this was last line
				code = read_code(line);

				col_count = 0;

				if((code != -1) && control_end(line)) {
					site->last_recv[out_cur_ofs] = '\0';
					site_idle(site);
					return code;
				}

				continue;
			}

			col_count++;
		}
	}

	site->last_recv[out_cur_ofs] = '\0';
	site_idle(site);
	return 0;
}

bool control_send(struct site_info *site, char *data) {
	return __control_send(site, data, false);
}

int32_t control_recv(struct site_info *site) {
	int32_t code = __control_recv(site, false);

	//connection timed out, kill socket
	if(code == 421) {
		cmd_execute(site->thread_id, EV_SITE_CLOSE, NULL);

		 struct msg *m = malloc(sizeof(struct msg));
		 m->to_id = THREAD_ID_UI;
		 m->from_id = site->thread_id;
		 m->event = EV_UI_RM_SITE;
		 msg_send(m);

		log_ui(site->thread_id, LOG_T_E, "%s: connection timed out.\n", site->name);
	}

	return code;
}

bool ftp_sscn(struct site_info *site, bool set_on) {
	if(site->sscn_on == set_on) {
		return true;
	}

	char *s_on = "SSCN ON\r\n";
	char *s_off = "SSCN OFF\r\n";

	char *s_cmd = set_on ? s_on : s_off;

	control_send(site, s_cmd);

	if(control_recv(site) == 200) {
		site->sscn_on = set_on;
		return true;
	}

	return false;
}

bool ftp_prot(struct site_info *site) {
	control_send(site, "PROT P\r\n");

	if(control_recv(site) == 200) {
		site->prot_sent = true;
		return true;
	}

	return false;
}

bool ftp_pwd(struct site_info *site) {
	//read pwd
	control_send(site, "PWD\r\n");
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

bool ftp_ls(struct site_info *site) {
	//get list
	control_send(site, "STAT -la\r\n");

	uint32_t code = control_recv(site);

	if((code != 213) && (code != 212)) {
		return false;
	}

	struct file_item *fl = parse_list(site->last_recv);
	file_item_destroy(site->cur_dirlist);
	site->cur_dirlist = fl;

	return true;
}

bool ftp_feat(struct site_info *site) {
	control_send(site, "FEAT\r\n");

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

	control_send(site, s);
	bool ret = control_recv(site) == 200;

	if(ret) {
		site->xdupe_enabled = true;
	}

	return ret;
}

bool auth(struct site_info *site) {
	int32_t code;
	char s_auth[SITE_USER_MAX+7];
	char s_pass[SITE_PASS_MAX+7];

	//init TLS connection if enabled
	if(site->use_tls) {
		__control_send(site, "AUTH TLS\r\n", true);
		code = __control_recv(site, true);

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

		if(SSL_connect(ssl) == -1) {
			log_w("TLS FAILED, ERROR:\n");
			ERR_print_errors_fp(stderr);
			SSL_CTX_free(ctx);
			return false;
		}

		site->secure_fd = ssl;
	}

	//send username
	snprintf(s_auth, SITE_USER_MAX+8, "USER %s\r\n", site->username);
	control_send(site, s_auth);
	code = control_recv(site);
	
	//username failed?
	if(code != 331) {
		ftp_disconnect(site);
		return false;
	}

	//send password
	snprintf(s_pass, SITE_PASS_MAX+8, "PASS %s\r\n", site->password);
	control_send(site, s_pass);
	code = control_recv(site);

	//password fail?
	if(code != 230) {
		ftp_disconnect(site);
		return false;
	}

	//set protection buffer to 0
	control_send(site, "PBSZ 0\r\n");
	code = control_recv(site);

	//set binary mode
	control_send(site, "TYPE I\r\n");
	code = control_recv(site);

	//set protection P
	ftp_prot(site);

	//get features
	ftp_feat(site);

	//enable xdupe if set in conf
	if(config_get_conf()->enable_xdupe) {
		ftp_xdupe(site, 3);
	}

	//get initial filelist && ret
	return ftp_ls(site) && ftp_pwd(site);
}

bool ftp_connect(struct site_info *site) {
	site->is_connecting = true;

	site_busy(site);
	int32_t sockfd = open_socket(site->address, site->port);

	if(sockfd == -1) {
		site_idle(site);
		site->is_connecting = false;
		return false;
	}
	
	site->socket_fd = sockfd;

	uint32_t code = __control_recv(site, true);

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
	bool ret = auth(site);

	site->is_connecting = false;
	return ret;
}

void ftp_disconnect(struct site_info *site) {
	close(site->socket_fd);
	log_w("FTP session was terminated\n");
}

struct pasv_details *ftp_pasv(struct site_info *site, bool handshake) {
	if(handshake) {
		control_send(site, "CPSV\r\n");
	} else {
		control_send(site, "PASV\r\n");
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

bool ftp_retr(struct site_info *site, char *filename) {
	int slen = strlen(filename)+9;
	char *s_retr = malloc(slen);

	snprintf(s_retr, slen, "RETR %s\r\n", filename);
	control_send(site, s_retr);

	free(s_retr);

	return control_recv(site) == 150;	
}

bool ftp_stor(struct site_info *site, char *filename) {
	int slen = strlen(filename)+9;
	char *s_retr = malloc(slen);

	snprintf(s_retr, slen, "STOR %s\r\n", filename);
	control_send(site, s_retr);

	free(s_retr);

	bool ret = control_recv(site) == 150;

	//if xdupe enabled and there are xdupe strings in output, parse it
	if(site->xdupe_enabled && (strstr(site->last_recv, "X-DUPE") != NULL)) {
		struct linked_str_node *xdupe_list = parse_xdupe(site->last_recv);
		struct linked_str_node *tmp_node;

		char *p_dir = str_get_path(filename);

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

bool ftp_open_data(struct site_info *site) {
	if(!site->prot_sent) {
		if(!ftp_prot(site)) {
			return false;
		}
	}

	struct pasv_details *pv = ftp_pasv(site, false);

	if(pv == NULL) {
    	return false;
	}

	char port[8];
	sprintf(port, "%d", pv->port);

	site->data_socket_fd = open_socket(pv->host, port);

	if(site->data_socket_fd == -1) {
		return false;
	}

	return true;
}

bool ftp_data_enable_tls(struct site_info *site) {
	SSL_CTX *ctx = ssl_get_context();

	if(ctx == NULL) {
		log_w("failed to get SSL context\n");
		close(site->data_socket_fd);
		return false;
	}

	SSL *ssl = SSL_new(ctx);
	SSL_set_fd(ssl, site->data_socket_fd);

	if(SSL_connect(ssl) == -1) {
		log_w("TLS FAILED, ERROR:\n");
		ERR_print_errors_fp(stderr);
		SSL_CTX_free(ctx);
		close(site->data_socket_fd);
		return false;
	}

	site->data_secure_fd = ssl;

	return true;
}

struct transfer_result *ftp_get(struct site_info *site, const char *filename, const char *local_dir, const char *remote_dir) {
	struct file_item *file = find_file(site->cur_dirlist, filename);
	struct timeval time_start;
	struct timeval time_end;

	if(file == NULL) {
		return transfer_result_create(false, strdup(filename), 0, 0.0f, false, FILE_TYPE_FILE);
	}


	struct file_item *loc_file = find_local_file(local_dir, filename);

	if(loc_file != NULL) {
		log_ui(site->thread_id, LOG_T_W, "%s: file exists, skipping\n", filename);
		free(loc_file);
		return transfer_result_create(true, strdup(filename), 0, 0.0f, true, FILE_TYPE_FILE);
	}

	free(loc_file);

	if(file->skip) {
		log_ui(site->thread_id, LOG_T_W, "%s: skip\n", filename);
		return transfer_result_create(true, strdup(filename), 0, 0.0f, true, FILE_TYPE_FILE);
	}

	//make sure sscn is off
	if(site->enable_sscn) {
		if(!ftp_sscn(site, false)) {
			return transfer_result_create(false, strdup(filename), 0, 0.0f, false, FILE_TYPE_FILE);
		}
	}


	if(!ftp_open_data(site)) {
		return transfer_result_create(false, strdup(filename), 0, 0.0f, false, FILE_TYPE_FILE);
	}

	char *new_lpath = path_append_file(local_dir, filename);
	char *new_rpath = path_append_file(remote_dir, filename);

	if(!ftp_retr(site, new_rpath)) {
		close(site->data_socket_fd);
		free(new_lpath);
		free(new_rpath);
		return transfer_result_create(false, strdup(filename), 0, 0.0f, false, FILE_TYPE_FILE);
	}

	if(site->use_tls) {
		if(!ftp_data_enable_tls(site)) {
			free(new_lpath);
			free(new_rpath);
			return transfer_result_create(false, strdup(filename), 0, 0.0f, false, FILE_TYPE_FILE);
		}
	}

	//create local path
	char buf[DATA_BUF_SZ];
	int32_t numbytes;
	FILE *fd = fopen(new_lpath, "wb");
	size_t tot_read = 0;
	
	free(new_lpath);
	free(new_rpath);
	
	if(fd == NULL) {
		log_w("error opening file for writing\n");
		close(site->data_socket_fd);
		return transfer_result_create(false, strdup(filename), 0, 0.0f, false, FILE_TYPE_FILE);
	}

	bool write_failed = false;

	site_busy(site);

	struct timeval time_last;
	gettimeofday(&time_last, NULL);
	gettimeofday(&time_start, NULL);

	while((numbytes = read_data_socket(site, buf, DATA_BUF_SZ-1, false)) != 0) {
		if (numbytes == -1) {
			log_w("error recv\n");
			write_failed = true;
			break;
		}

		tot_read += numbytes;
		
		if(fwrite(buf, sizeof(char), numbytes, fd) != numbytes) {
			write_failed = true;
			site->current_speed = 0.0;
			break;
		}

		//update speed info
		gettimeofday(&time_end, NULL);

		if((time_end.tv_sec - time_last.tv_sec) >= 1) {
			site->current_speed = calc_transfer_speed(&time_start, &time_end, tot_read);
			time_last.tv_sec = time_end.tv_sec;
		}
	}

	//reset transfer speed
	site->current_speed = 0.0;
	gettimeofday(&time_end, NULL);

	if(site->use_tls) {
		//important to make sure the ssl shutdown has finished before closing sock
		while(SSL_shutdown(site->data_secure_fd) == 0) {
		}
		SSL_free(site->data_secure_fd);
	}

	fclose(fd);
	close(site->data_socket_fd);

	uint32_t code = control_recv(site);

	if(write_failed) {
		return transfer_result_create(false, strdup(filename), 0, 0.0f, false, FILE_TYPE_FILE);
	}

	if(code == 226) {
		//calculate speed on succ. transfer 
		char *speed = s_calc_transfer_speed(&time_start, &time_end, tot_read);
		char *s_size = parse_file_size(tot_read);

		log_ui(site->thread_id, LOG_T_I, "%s: downloaded %s at %s in %ds\n", filename, s_size, speed, time_end.tv_sec - time_start.tv_sec);

		free(s_size);
		free(speed);
		return transfer_result_create(true, strdup(filename), tot_read, calc_transfer_speed(&time_start, &time_end, tot_read), false, FILE_TYPE_FILE);
	} else {
		return transfer_result_create(false, strdup(filename), 0, 0.0f, false, FILE_TYPE_FILE);
	}
}

struct transfer_result *ftp_put(struct site_info *site, const char *filename, const char *local_dir, const char *remote_dir) {
	struct file_item *local_file = find_local_file(local_dir, filename);
	struct timeval time_start;
	struct timeval time_end;

	if(local_file == NULL) {
		return transfer_result_create(false, strdup(filename), 0, 0.0f, false, FILE_TYPE_FILE);
	}

	if(local_file->skip) {
		log_ui(site->thread_id, LOG_T_W, "%s: skip\n", filename);
		free(local_file);
		return transfer_result_create(true, strdup(filename), 0, 0.0f, true, FILE_TYPE_FILE);
	}

	free(local_file);

	char *new_rpath = path_append_file(remote_dir, filename);

	if(site->xdupe_enabled) {
		if(site_xdupe_has(site, new_rpath)) {
			log_ui(site->thread_id, LOG_T_W, "%s: skip (x-dupe)\n", filename);
			free(new_rpath);
			return transfer_result_create(true, strdup(filename), 0, 0.0f, true, FILE_TYPE_FILE);
		}
	}

	//make sure sscn is off
	if(site->enable_sscn) {
		if(!ftp_sscn(site, false)) {
			free(new_rpath);
			return transfer_result_create(false, strdup(filename), 0, 0.0f, false, FILE_TYPE_FILE);
		}
	}

	if(!ftp_open_data(site)) {
		free(new_rpath);
		return transfer_result_create(false, strdup(filename), 0, 0.0f, false, FILE_TYPE_FILE);
	}

	char *new_lpath = path_append_file(local_dir, filename);

	if(!ftp_stor(site, new_rpath)) {
		close(site->data_socket_fd);
		free(new_lpath);
		free(new_rpath);
		return transfer_result_create(false, strdup(filename), 0, 0.0f, false, FILE_TYPE_FILE);
	}

	site_busy(site);

	if(site->use_tls) {
		if(!ftp_data_enable_tls(site)) {
			free(new_lpath);
			free(new_rpath);
			return transfer_result_create(false, strdup(filename), 0, 0.0f, false, FILE_TYPE_FILE);
		}
	}

	//create local path
	char buf[DATA_BUF_SZ];
	FILE *fd = fopen(new_lpath, "rb");
	uint32_t n_sent = 0;
	uint32_t n = 0;
	size_t n_read = 0;
	size_t tot_sent = 0;
	//long int len;
	
	free(new_lpath);
	free(new_rpath);

	if(fd == NULL) {
		log_w("error opening file for reading\n");
		close(site->data_socket_fd);
		return transfer_result_create(false, strdup(filename), 0, 0.0f, false, FILE_TYPE_FILE);
	}

	//fseek(fd, 0L, SEEK_END);
	//len = ftell(fd);
	//rewind(fd);

	struct timeval time_last;
	gettimeofday(&time_last, NULL);
	gettimeofday(&time_start, NULL);

	bool reading = true;
	while(reading) {
		//read block
		n_read = fread(buf, 1, DATA_BUF_SZ, fd);
		n_sent = 0;
		//data read not expected size
		if(n_read != DATA_BUF_SZ) {
			//check if error set or not EOF
			if( (ferror(fd) != 0) || (feof(fd) == 0) ) {
				log_w("error reading data\n");
				fclose(fd);
				close(site->data_socket_fd);
				return transfer_result_create(false, strdup(filename), 0, 0.0f, false, FILE_TYPE_FILE);
			}
			reading = false;
		}

		//send block			
		while(n_sent < n_read) {
			n = write_data_socket(site, buf+n_sent, n_read-n_sent, false);
			n_sent += n;
			tot_sent += n;

			if(n == -1) {
				log_w("error writing data\n");
				fclose(fd);
				close(site->data_socket_fd);
				return transfer_result_create(false, strdup(filename), 0, 0.0f, false, FILE_TYPE_FILE);
			}
		}

		//update speed info
		gettimeofday(&time_end, NULL);

		if((time_end.tv_sec - time_last.tv_sec) >= 1) {
			site->current_speed = calc_transfer_speed(&time_start, &time_end, tot_sent);
			time_last.tv_sec = time_end.tv_sec;
		}
	}

	site->current_speed = 0.0;
	gettimeofday(&time_end, NULL);

	if(site->use_tls) {
		//important to make sure the ssl shutdown has finished before closing sock
		while(SSL_shutdown(site->data_secure_fd) == 0) {
		}
		SSL_free(site->data_secure_fd);
	}

	fclose(fd);
	close(site->data_socket_fd);

	if(control_recv(site) == 226) {
		//calculate speed on succ. transfer
		char *speed = s_calc_transfer_speed(&time_start, &time_end, tot_sent);
        char *s_size = parse_file_size(tot_sent);

		log_ui(site->thread_id, LOG_T_I, "%s: uploaded %s at %s in %ds\n", filename, s_size, speed, time_end.tv_sec - time_start.tv_sec);

		free(s_size);
		free(speed);
		return transfer_result_create(true, strdup(filename), tot_sent, calc_transfer_speed(&time_start, &time_end, tot_sent), false, FILE_TYPE_FILE);
	} else {
		return transfer_result_create(false, strdup(filename), 0, 0.0f, false, FILE_TYPE_FILE);
	}
}

bool ftp_cwd(struct site_info *site, const char *dirname) {
	int cmd_len = strlen(dirname) + 7;
	char *s_cmd = malloc(cmd_len);

	snprintf(s_cmd, cmd_len, "CWD %s\r\n", dirname);

	control_send(site, s_cmd);
	free(s_cmd);

	return (control_recv(site) == 250) && ftp_pwd(site);

}

bool ftp_cwd_up(struct site_info *site) {
	return ftp_cwd(site, "..") && ftp_ls(site);
}

bool ftp_mkd(struct site_info *site, char *dirname) {
	int cmd_len = strlen(dirname) + 7;
	char *s_cmd = malloc(cmd_len);

	snprintf(s_cmd, cmd_len, "MKD %s\r\n", dirname);

	control_send(site, s_cmd);
	free(s_cmd);

	return control_recv(site) == 257;
}

struct transfer_result *__ftp_get_recursive(struct site_info *site, const char *dirname, const char *local_dir, const char *remote_dir) {
	//make copy of dirname, local_dir, remote_dir to prevent cwd from manipulating ptr
	char *_dirname = strdup(dirname);
	char *_local_dir = strdup(local_dir);
	char *_remote_dir = strdup(remote_dir);

	struct file_item *file = find_file(site->cur_dirlist, _dirname);

	if(file == NULL) {
		return transfer_result_create(false, strdup(_dirname), 0, 0.0f, false, FILE_TYPE_DIR);
	}

	if(file->skip) {
		log_ui(site->thread_id, LOG_T_W, "%s: skip\n", _dirname);
		return transfer_result_create(true, strdup(_dirname), 0, 0.0f, true, FILE_TYPE_DIR);
	}

	char *new_lpath = path_append_dir(_local_dir, _dirname);
	char *new_rpath = path_append_dir(_remote_dir, _dirname);

	if(!ftp_cwd(site, new_rpath)) {
		return transfer_result_create(false, strdup(_dirname), 0, 0.0f, false, FILE_TYPE_DIR);
	}

	if(!file_exists(new_lpath)) {
		log_ui(site->thread_id, LOG_T_I, "%s: creating dir\n", new_lpath);
		if(mkdir(new_lpath, 0755) != 0) {
			log_ui(site->thread_id, LOG_T_E, "%s: unable to mkdir!\n", new_lpath, FILE_TYPE_DIR);
			//ftp_cwd(site, "..");
			//ftp_ls(site);
			free(new_lpath);
			free(new_rpath);
			return transfer_result_create(false, strdup(_dirname), 0, 0.0f, false, FILE_TYPE_DIR);
		}
	}

	if(!ftp_ls(site)) {
		log_ui(site->thread_id, LOG_T_E, "%s: unable to get dirlist!\n", _dirname);
		free(new_lpath);
		free(new_rpath);
		return transfer_result_create(false, strdup(dirname), 0, 0.0f, false, FILE_TYPE_DIR);
	}

	if(site->cur_dirlist == NULL) {
		log_ui(site->thread_id, LOG_T_W, "%s: empty dir\n", _dirname);
		//ftp_cwd(site, ".."); // go back
		//ftp_ls(site);
		free(new_lpath);
		free(new_rpath);
		return transfer_result_create(true, strdup(_dirname), 0, 0.0f, true, FILE_TYPE_DIR);
	}

	struct file_item *fl = site->cur_dirlist;

	struct transfer_result *ret = transfer_result_create(true, strdup(_dirname), 0, 0.0f, false, FILE_TYPE_DIR);
	struct transfer_result *rp = ret;

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
	struct file_item *first_file = file_item_cpy(site->cur_dirlist);
	fl = first_file;

	while(fl != NULL) {
		//move to end of ret list
		while(rp->next != NULL) {
			rp = rp->next;
		}

		if(fl->file_type == FILE_TYPE_FILE) {
			cur_file_num++;

			log_ui(site->thread_id, LOG_T_I, "%s: downloading file [%d/%d]\n", fl->file_name, cur_file_num, tot_files);

			struct transfer_result *f_ret = ftp_get(site, fl->file_name, new_lpath, new_rpath);

			if(!f_ret->success) {
				log_ui(site->thread_id, LOG_T_E, "%s: download failed!\n", fl->file_name);
			}

			//append stat
			rp->next = f_ret;

		} else if(fl->file_type == FILE_TYPE_DIR) {
			log_ui(site->thread_id, LOG_T_I, "%s: downloading dir..\n", fl->file_name);

			rp->next = __ftp_get_recursive(site, fl->file_name, new_lpath, new_rpath);
			
			//restore current path
			if(!ftp_cwd(site, new_rpath)) {
				//mark dir as failed
				ret->success = false;
				break;
			}

			if(!ftp_ls(site)) {
				ret->success = false;
				break;
			}
		}

		fl = fl->next;
	}

	file_item_destroy(first_file);
	
	free(new_lpath);
	free(new_rpath);

	free(_local_dir);
	free(_remote_dir);
	free(_dirname);

	return ret;
}

struct transfer_result *ftp_get_recursive(struct site_info *site, const char *dirname, const char *local_dir, const char *remote_dir) {
	struct timeval time_start;
	struct timeval time_end;

	gettimeofday(&time_start, NULL);

	char *remote_origin = strdup(remote_dir);
	struct transfer_result *ret = __ftp_get_recursive(site, dirname, local_dir, remote_dir);

	gettimeofday(&time_end, NULL);

	ftp_cwd(site, remote_origin);
	ftp_ls(site);

	log_ui(site->thread_id, LOG_T_I, "Stats: %s\n", s_gen_stats(ret, time_end.tv_sec - time_start.tv_sec));

	free(remote_origin);

	return ret; 
}

struct transfer_result *__ftp_put_recursive(struct site_info *site, const char *dirname, const char *local_dir, const char *remote_dir) {
	//copy args to prevent outside changes during cwd etc if ptr from dirlist etc
	char *_dirname = strdup(dirname);
	char *_local_dir = strdup(local_dir);
	char *_remote_dir = strdup(remote_dir);

	struct file_item *local_file = find_local_file(_local_dir, _dirname);

	if(local_file == NULL) {
		return transfer_result_create(false, strdup(_dirname), 0, 0.0f, false, FILE_TYPE_DIR);
	}

	if(local_file->skip) {
		log_ui(site->thread_id, LOG_T_W, "%s: skip\n", _dirname);
		free(local_file);
		return transfer_result_create(true, strdup(_dirname), 0, 0.0f, true, FILE_TYPE_DIR);
	}

	free(local_file);

	char *new_lpath = path_append_dir(_local_dir, _dirname);
	char *new_rpath = path_append_dir(_remote_dir, _dirname);
	struct file_item *rfile = find_file(site->cur_dirlist, _dirname);

	if(rfile == NULL) {
		if(!ftp_mkd(site, new_rpath)) {
			log_ui(site->thread_id, LOG_T_E, "%s: mkdir failed!\n", _dirname);
		}
	} else if(rfile->file_type != FILE_TYPE_DIR) {
		log_ui(site->thread_id, LOG_T_E, "%s: remote file not a directory!\n", _dirname);
		free(new_lpath);
		free(new_rpath);
		return transfer_result_create(false, strdup(_dirname), 0, 0.0f, false, FILE_TYPE_DIR);
	}

	if(!ftp_cwd(site, new_rpath)) {
		free(new_lpath);
		free(new_rpath);
		return transfer_result_create(false, strdup(_dirname), 0, 0.0f, false, FILE_TYPE_DIR);
	}

	if(!ftp_ls(site)) {
		free(new_lpath);
		free(new_rpath);
		return transfer_result_create(false, strdup(_dirname), 0, 0.0f, false, FILE_TYPE_DIR);
	}

	struct file_item *l_list = local_ls(new_lpath, true);
	struct file_item *lp = l_list;

	if(lp == NULL) {
		log_ui(site->thread_id, LOG_T_W, "%s: empty dir\n", _dirname);
		return transfer_result_create(true, strdup(_dirname), 0, 0.0f, true, FILE_TYPE_DIR);
	}

	struct transfer_result *ret = transfer_result_create(true, strdup(_dirname), 0, 0.0f, false, FILE_TYPE_DIR);
	struct transfer_result *rp = ret;

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
			log_ui(site->thread_id, LOG_T_I, "%s: uploading file [%d/%d]\n", lp->file_name, cur_file_num, tot_files);

			struct transfer_result *f_ret = ftp_put(site, lp->file_name, new_lpath, new_rpath);

			if(!f_ret->success) {
				log_ui(site->thread_id, LOG_T_E, "%s: upload failed!\n", lp->file_name);
			}

			//append stat
			rp->next = f_ret;
		} else if(lp->file_type == FILE_TYPE_DIR) {
			log_ui(site->thread_id, LOG_T_I, "%s: uploading dir..\n", lp->file_name);

			rp->next = __ftp_put_recursive(site, lp->file_name, new_lpath, new_rpath);

			//restore current path
			if(!ftp_cwd(site, new_rpath)) {
				//mark dir as failed
				ret->success = false;
				break;
			}

			if(!ftp_ls(site)) {
				ret->success = false;
				break;
			}

		}

		lp = lp->next;
	}

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

	return ret;
}

struct transfer_result *ftp_put_recursive(struct site_info *site, const char *dirname, const char *local_dir, const char *remote_dir) {
	struct timeval time_start;
	struct timeval time_end;

	gettimeofday(&time_start, NULL);

	char *remote_origin = strdup(remote_dir);
	struct transfer_result *ret = __ftp_put_recursive(site, dirname, local_dir, remote_dir);

	gettimeofday(&time_end, NULL);

	ftp_cwd(site, remote_origin);
	ftp_ls(site);
	
	log_ui(site->thread_id, LOG_T_I, "Stats: %s\n", s_gen_stats(ret, time_end.tv_sec - time_start.tv_sec));

	free(remote_origin);

	return ret;
}

struct transfer_result *fxp(struct site_info *src, struct site_info *dst, const char *filename, const char *src_dir, const char *dst_dir) {
	struct file_item *file = find_file(src->cur_dirlist, filename);
	struct timeval time_start;
	struct timeval time_end;

	bool use_sscn = src->enable_sscn && dst->enable_sscn;

	if(file == NULL) {
		return transfer_result_create(false, strdup(filename), 0, 0.0f, false, FILE_TYPE_FILE);
	}

	if(file->skip) {
		log_ui(src->thread_id, LOG_T_W, "%s: skip\n", filename);
		return transfer_result_create(true, strdup(filename), 0, 0.0f, true, FILE_TYPE_FILE);
	}

	char *new_dpath = path_append_file(dst_dir, filename);

	if(dst->xdupe_enabled) {
		if(site_xdupe_has(dst, new_dpath)) {
			log_ui(dst->thread_id, LOG_T_W, "%s: skip (x-dupe)\n", filename);
			free(new_dpath);
			return transfer_result_create(true, strdup(filename), 0, 0.0f, true, FILE_TYPE_FILE);
		}
	}


	if(use_sscn) {
		if(!ftp_sscn(src, !src->enforce_sscn_server_mode)) {
			free(new_dpath);
			return transfer_result_create(false, strdup(filename), 0, 0.0f, false, FILE_TYPE_FILE);
		}

		if(!ftp_sscn(dst, src->enforce_sscn_server_mode)) {
			free(new_dpath);
			return transfer_result_create(false, strdup(filename), 0, 0.0f, false, FILE_TYPE_FILE);
		}
	}
	

	struct pasv_details *pv = ftp_pasv(src, src->use_tls && !use_sscn);

	if(pv == NULL) {
		free(new_dpath);
		return transfer_result_create(false, strdup(filename), 0, 0.0f, false, FILE_TYPE_FILE);
	}

	uint32_t s_len = strlen(pv->unparsed) + 9;
	char *s_port = malloc(s_len);
	snprintf(s_port, s_len, "PORT %s\r\n", pv->unparsed);

	control_send(dst, s_port);
	if(control_recv(dst) != 200) {
		free(new_dpath);
		return transfer_result_create(false, strdup(filename), 0, 0.0f, false, FILE_TYPE_FILE);
	}

	free(s_port);

	char *new_spath = path_append_file(src_dir, filename);

	if(!ftp_stor(dst, new_dpath)) {
		free(new_spath);
		free(new_dpath);
		return transfer_result_create(false, strdup(filename), 0, 0.0f, false, FILE_TYPE_FILE);
	}

	//also mark dst as busy
	site_busy(dst);

	gettimeofday(&time_start, NULL);

	if(!ftp_retr(src, new_spath)) {
		//if retr fails, cancel the STOR with pasv trick
		control_send(src, "PASV\r\n");
		control_recv(src);

		//wait for STOR resp
		control_recv(dst);
		free(new_spath);
		free(new_dpath);
		return transfer_result_create(false, strdup(filename), 0, 0.0f, false, FILE_TYPE_FILE);
	}

	if(control_recv(src) != 226) {
		free(new_spath);
		free(new_dpath);
		return transfer_result_create(false, strdup(filename), 0, 0.0f, false, FILE_TYPE_FILE);
	}

	if(control_recv(dst) != 226) {
		free(new_spath);
		free(new_dpath);
		return transfer_result_create(false, strdup(filename), 0, 0.0f, false, FILE_TYPE_FILE);
	}

	free(new_spath);
	free(new_dpath);

	gettimeofday(&time_end, NULL);
	char *speed = s_calc_transfer_speed(&time_start, &time_end, file->size);
	char *s_size = parse_file_size(file->size);

	log_ui(src->thread_id, LOG_T_I, "%s: fxp'd %s at %s in %ds\n", filename, s_size, speed, time_end.tv_sec - time_start.tv_sec);	

	free(s_size);
	return transfer_result_create(true, strdup(filename), file->size, calc_transfer_speed(&time_start, &time_end, file->size), false, FILE_TYPE_FILE);
}

struct transfer_result *__fxp_recursive(struct site_info *src, struct site_info *dst, const char *dirname, const char *src_dir, const char *dst_dir) {
	//copy args for safety
	char *_dirname = strdup(dirname);
	char *_src_dir = strdup(src_dir);
	char *_dst_dir = strdup(dst_dir);

	struct file_item *file = find_file(src->cur_dirlist, _dirname);

	if(file == NULL) {
		return transfer_result_create(false, strdup(_dirname), 0, 0.0f, false, FILE_TYPE_DIR);
	}

	if(file->skip) {
		log_ui(src->thread_id, LOG_T_W, "%s: skip\n", _dirname);
		return transfer_result_create(true, strdup(_dirname), 0, 0.0f, true, FILE_TYPE_DIR);
	}

	char *new_spath = path_append_dir(_src_dir, _dirname);
	char *new_dpath = path_append_dir(_dst_dir, _dirname);

	struct file_item *rfile = find_file(dst->cur_dirlist, _dirname);

	if(rfile == NULL) {
		if(!ftp_mkd(dst, new_dpath)) {
			log_ui(dst->thread_id, LOG_T_E, "%s: mkdir failed!\n", _dirname);
			free(new_spath);
			free(new_dpath);
			return transfer_result_create(false, strdup(_dirname), 0, 0.0f, false, FILE_TYPE_DIR);
		}
	} else if(rfile->file_type != FILE_TYPE_DIR) {
		log_ui(dst->thread_id, LOG_T_E, "%s: remote file not a directory!\n", _dirname);
		free(new_spath);
		free(new_dpath);
		return transfer_result_create(false, strdup(_dirname), 0, 0.0f, false, FILE_TYPE_DIR);
	}

	//cwd and list src
	if(!ftp_cwd(src, _dirname)) {
		free(new_spath);
		free(new_dpath);
		return transfer_result_create(false, strdup(_dirname), 0, 0.0f, false, FILE_TYPE_DIR);
	}

	if(!ftp_ls(src)) {
		//ftp_cwd_up(src);
		free(new_spath);
		free(new_dpath);
		return transfer_result_create(false, strdup(_dirname), 0, 0.0f, false, FILE_TYPE_DIR);
	}

	//cwd and list dst
	if(!ftp_cwd(dst, _dirname)) {
		//ftp_cwd_up(src);
		free(new_spath);
		free(new_dpath);
		return transfer_result_create(false, strdup(_dirname), 0, 0.0f, false, FILE_TYPE_DIR);
	}

	if(!ftp_ls(dst)) {
		//ftp_cwd_up(src);
		//ftp_cwd_up(dst);
		free(new_spath);
		free(new_dpath);
		return transfer_result_create(false, strdup(_dirname), 0, 0.0f, false, FILE_TYPE_DIR);
	}

	struct file_item *fl = src->cur_dirlist;

	if(fl == NULL) {
		log_ui(src->thread_id, LOG_T_W, "%s: empty dir\n", _dirname);
		free(new_spath);
		free(new_dpath);
		return transfer_result_create(true, strdup(_dirname), 0, 0.0f, true, FILE_TYPE_DIR);
	}

	struct transfer_result *ret = transfer_result_create(true, strdup(_dirname), 0, 0.0f, false, FILE_TYPE_DIR);
	 struct transfer_result *rp = ret;	

	//count files in list
	uint32_t tot_files = 0;
	uint32_t cur_file_num = 0;

	while(fl != NULL) {
		if(fl->file_type == FILE_TYPE_FILE) {
			tot_files++;
		}
		fl = fl->next;
	}

	//make a copy to prevent cwd clear
	struct file_item *first_file = file_item_cpy(src->cur_dirlist);
	fl = first_file;

	while(fl != NULL) {
		//move to end of ret list
		while(rp->next != NULL) {
			rp = rp->next;
		}

		if(fl->file_type == FILE_TYPE_FILE) {
			cur_file_num++;
			log_ui(src->thread_id, LOG_T_I, "%s: fxp'ing file [%d/%d]\n", fl->file_name, cur_file_num, tot_files);

			struct transfer_result *f_ret = fxp(src, dst, fl->file_name, new_spath, new_dpath);

			if(!f_ret->success) {
				log_ui(src->thread_id, LOG_T_E, "%s: fxp failed!\n", fl->file_name);
			}

			rp->next = f_ret;
		} else if(fl->file_type == FILE_TYPE_DIR) {
			log_ui(src->thread_id, LOG_T_I, "%s: fxp'ing dir..\n", fl->file_name);

			rp->next = __fxp_recursive(src, dst, fl->file_name, new_spath, new_dpath);

			//restore current path
			if(!ftp_cwd(src, new_spath)) {
				//mark dir as failed
				ret->success = false;
				break;
			}

			if(!ftp_ls(src)) {
				ret->success = false;
				break;
			}

			//restore current path
			if(!ftp_cwd(dst, new_dpath)) {
				//mark dir as failed
				ret->success = false;
				break;
			}

			if(!ftp_ls(dst)) {
				ret->success = false;
				break;
			}
		}

		fl = fl->next;
	}

	file_item_destroy(first_file);

	free(new_spath);
	free(new_dpath);
	free(_dirname);
	free(_src_dir);
	free(_dst_dir);
	return ret;
}

struct transfer_result *fxp_recursive(struct site_info *src, struct site_info *dst, const char *dirname, const char *src_dir, const char *dst_dir) {
	struct timeval time_start;
	struct timeval time_end;

	gettimeofday(&time_start, NULL);

	char *src_origin = strdup(src_dir);
	char *dst_origin = strdup(dst_dir);
	struct transfer_result *ret = __fxp_recursive(src, dst, dirname, src_dir, dst_dir);

	gettimeofday(&time_end, NULL);

	ftp_cwd(src, src_origin);
	ftp_cwd(dst, dst_origin);
	ftp_ls(src);
	ftp_ls(dst);

	log_ui(src->thread_id, LOG_T_I, "Stats: %s\n", s_gen_stats(ret, time_end.tv_sec - time_start.tv_sec));

	free(src_origin);
	free(dst_origin);

	return ret;
}
