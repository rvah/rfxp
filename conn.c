#include "conn.h"

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

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

	log_w("%s",data);

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
	return __control_recv(site, false);
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

	//get initial filelist && ret
	return ftp_ls(site) && ftp_pwd(site);
}

int32_t open_socket(char *address, char *port) {
	int32_t sockfd;
	struct addrinfo hints, *servinfo, *p;
	int32_t rv;
	char s[INET6_ADDRSTRLEN];

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(address, port, &hints, &servinfo)) != 0) {
		log_w("getaddrinfo: %s\n", gai_strerror(rv));
		return -1;
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			log_w("error client: socket\n");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			log_w("error client: connect\n");
			continue;
		}

		break;
	}

	if (p == NULL) {
		log_w("socket: failed to connect\n");
		return -1;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
	log_w("client: connecting to %s\n", s);

	freeaddrinfo(servinfo); // all done with this structure
	return sockfd;
}

bool ftp_connect(struct site_info *site) {
	int32_t sockfd = open_socket(site->address, site->port);

	if(sockfd == -1) {
		return false;
	}
	
	site->socket_fd = sockfd;

	uint32_t code = __control_recv(site, true);

	if(code != 220) {
		ftp_disconnect(site);
		return false;
	}

	//proftpd seems troublesome with sscn
	//if proftpd identified, always set SSCN ON(client mode) on the other server
	//TODO: figure out if proftpd issue, or if its just my test server
	if(strstr(site->last_recv, "ProFTPD") != NULL) {
		site->enforce_sscn_server_mode = true;
	}

	//do auth
	return auth(site);
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

	return control_recv(site) == 150;
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

bool ftp_get(struct site_info *site, char *filename, char *local_dir, char *remote_dir) {
	struct file_item *file = find_file(site->cur_dirlist, filename);
	struct timeval time_start;
	struct timeval time_end;

	if(file == NULL) {
		return false;
	}


	struct file_item *loc_file = find_local_file(local_dir, filename);

	if(loc_file != NULL) {
		log_ui(site->thread_id, LOG_T_W, "%s: file exists, skipping\n", filename);
		free(loc_file);
		return true;
	}

	free(loc_file);

	if(file->skip) {
		log_ui(site->thread_id, LOG_T_W, "%s: skip\n", filename);
		return true;
	}

	//make sure sscn is off
	if(site->enable_sscn) {
		if(!ftp_sscn(site, false)) {
			return false;
		}
	}


	if(!ftp_open_data(site)) {
		return false;
	}

	char *new_lpath = path_append_file(local_dir, filename);
	char *new_rpath = path_append_file(remote_dir, filename);

	if(!ftp_retr(site, new_rpath)) {
		close(site->data_socket_fd);
		free(new_lpath);
		free(new_rpath);
		return false;
	}

	if(site->use_tls) {
		if(!ftp_data_enable_tls(site)) {
			free(new_lpath);
			free(new_rpath);
			return false;
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
		return false;
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
		return false;
	}

	if(code == 226) {
		//calculate speed on succ. transfer 
		char *speed = s_calc_transfer_speed(&time_start, &time_end, tot_read);

		log_ui(site->thread_id, LOG_T_I, "%s downloaded at %s in %ds\n", filename, speed, time_end.tv_sec - time_start.tv_sec);
		free(speed);
		return true;
	} else {
		return false;
	}
}

bool ftp_put(struct site_info *site, char *filename, char *local_dir, char *remote_dir) {
	struct file_item *local_file = find_local_file(local_dir, filename);
	struct timeval time_start;
	struct timeval time_end;

	if(local_file == NULL) {
		return false;
	}

	if(local_file->skip) {
		log_ui(site->thread_id, LOG_T_W, "%s: skip\n", filename);
		free(local_file);
		return true;
	}

	free(local_file);

	//make sure sscn is off
	if(site->enable_sscn) {
		if(!ftp_sscn(site, false)) {
			return false;
		}
	}

	if(!ftp_open_data(site)) {
		return false;
	}

	char *new_lpath = path_append_file(local_dir, filename);
	char *new_rpath = path_append_file(remote_dir, filename);

	if(!ftp_stor(site, new_rpath)) {
		close(site->data_socket_fd);
		free(new_lpath);
		free(new_rpath);
		return false;
	}

	site_busy(site);

	if(site->use_tls) {
		if(!ftp_data_enable_tls(site)) {
			free(new_lpath);
			free(new_rpath);
			return false;
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
		return false;
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
				return false;
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
				return false;
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

		log_ui(site->thread_id, LOG_T_I, "%s uploaded at %s in %ds\n", filename, speed, time_end.tv_sec - time_start.tv_sec);
		free(speed);
		return true;
	} else {
		return false;
	}
}

bool ftp_cwd(struct site_info *site, char *dirname) {
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

bool ftp_get_recursive(struct site_info *site, char *dirname, char *local_dir, char *remote_dir) {
	struct file_item *file = find_file(site->cur_dirlist, dirname);

	if(file == NULL) {
		return false;
	}

	if(file->skip) {
		log_ui(site->thread_id, LOG_T_W, "%s: skip\n", dirname);
		return true;
	}

	char *new_lpath = path_append_dir(local_dir, dirname);
	char *new_rpath = path_append_dir(remote_dir, dirname);

	if(!ftp_cwd(site, new_rpath)) {
		return false;
	}

	if(!file_exists(new_lpath)) {
		log_ui(site->thread_id, LOG_T_I, "%s: creating dir\n", new_lpath);
		if(mkdir(new_lpath, 0755) != 0) {
			log_ui(site->thread_id, LOG_T_E, "%s: unable to mkdir!\n", new_lpath);
			ftp_cwd(site, "..");
			ftp_ls(site);
			free(new_lpath);
			free(new_rpath);
			return false;
		}
	}

	if(!ftp_ls(site)) {
		log_ui(site->thread_id, LOG_T_E, "%s: unable to get dirlist!\n", dirname);
		free(new_lpath);
		free(new_rpath);
		return false;
	}

	if(site->cur_dirlist == NULL) {
		log_ui(site->thread_id, LOG_T_W, "%s: empty dir\n", dirname);
		ftp_cwd(site, ".."); // go back
		ftp_ls(site);
		free(new_lpath);
		free(new_rpath);
		return true;
	}

	struct file_item *fl = site->cur_dirlist;
	bool flag_failed = false;

	while(fl != NULL) {
		if(fl->file_type == FILE_TYPE_FILE) {
			log_ui(site->thread_id, LOG_T_I, "%s: downloading file..\n", fl->file_name);
			if(!ftp_get(site, fl->file_name, new_lpath, new_rpath)) {
				log_ui(site->thread_id, LOG_T_E, "%s: download failed!\n", fl->file_name);
				flag_failed = true;
			}
		} else if(fl->file_type == FILE_TYPE_DIR) {
			log_ui(site->thread_id, LOG_T_I, "%s: downloading dir..\n", fl->file_name);
			if(!ftp_get_recursive(site, fl->file_name, new_lpath, new_rpath)) {
				log_ui(site->thread_id, LOG_T_E, "%s/: download failed!\n", fl->file_name);
				flag_failed = true;
			}
		}
		fl = fl->next;
	}	
	
	free(new_lpath);
	free(new_rpath);
	return ftp_cwd(site, "..") && ftp_ls(site) && !flag_failed;
}

bool ftp_put_recursive(struct site_info *site, char *dirname, char *local_dir, char *remote_dir) {
	struct file_item *local_file = find_local_file(local_dir, dirname);

	if(local_file == NULL) {
		return false;
	}

	if(local_file->skip) {
		log_ui(site->thread_id, LOG_T_W, "%s: skip\n", dirname);
		free(local_file);
		return true;
	}

	free(local_file);

	char *new_lpath = path_append_dir(local_dir, dirname);
	char *new_rpath = path_append_dir(remote_dir, dirname);
	struct file_item *rfile = find_file(site->cur_dirlist, dirname);

	if(rfile == NULL) {
		if(!ftp_mkd(site, new_rpath)) {
			log_ui(site->thread_id, LOG_T_E, "%s: mkdir failed!\n", dirname);
		}
	} else if(rfile->file_type != FILE_TYPE_DIR) {
		log_ui(site->thread_id, LOG_T_E, "%s: remote file not a directory!\n", dirname);
		free(new_lpath);
		free(new_rpath);
		return false;
	}

	if(!ftp_cwd(site, new_rpath)) {
		free(new_lpath);
		free(new_rpath);
		return false;
	}

	if(!ftp_ls(site)) {
		free(new_lpath);
		free(new_rpath);
		return false;
	}

	struct file_item *l_list = local_ls(new_lpath);
	struct file_item *lp = l_list;

	if(lp == NULL) {
		log_ui(site->thread_id, LOG_T_W, "%s: empty dir\n", dirname);
		return true;
	}

	bool flag_failed = false;

	while(lp != NULL) {
		//log_ui(site->thread_id, LOG_T_I, ">>%s<<%s>>%s<<\n", local_dir, dirname,lp->file_name);
		if(lp->file_type == FILE_TYPE_FILE) {
			log_ui(site->thread_id, LOG_T_I, "%s: uploading file..\n", lp->file_name);
			if(!ftp_put(site, lp->file_name, new_lpath, new_rpath)) {
				log_ui(site->thread_id, LOG_T_E, "%s: upload failed!\n", lp->file_name);
				flag_failed = true;
			}
		} else if(lp->file_type == FILE_TYPE_DIR) {
			log_ui(site->thread_id, LOG_T_I, "%s: uploading dir..\n", lp->file_name);
			if(!ftp_put_recursive(site, lp->file_name, new_lpath, new_rpath)) {
				log_ui(site->thread_id, LOG_T_E, "%s/: upload failed!\n", lp->file_name);
				flag_failed = true;
			}
		}

		lp = lp->next;
	}

	free(new_lpath);
	free(new_rpath);

	//free dir data
	while(l_list != NULL) {
		lp = l_list;
		l_list = l_list->next;
		free(lp);
	}

	return ftp_cwd(site, "..") && ftp_ls(site) && !flag_failed;
}

bool fxp(struct site_info *src, struct site_info *dst, char *filename, char *src_dir, char *dst_dir) {
	struct file_item *file = find_file(src->cur_dirlist, filename);
	struct timeval time_start;
	struct timeval time_end;

	bool use_sscn = src->enable_sscn && dst->enable_sscn;

	if(file == NULL) {
		return false;
	}

	if(file->skip) {
		log_ui(src->thread_id, LOG_T_W, "%s: skip\n", filename);
		return true;
	}

	if(use_sscn) {
		if(!ftp_sscn(src, !src->enforce_sscn_server_mode)) {
			return false;
		}

		if(!ftp_sscn(dst, src->enforce_sscn_server_mode)) {
			return false;
		}
	}
	

	struct pasv_details *pv = ftp_pasv(src, src->use_tls && !use_sscn);

	if(pv == NULL) {
		return false;
	}

	uint32_t s_len = strlen(pv->unparsed) + 9;
	char *s_port = malloc(s_len);
	snprintf(s_port, s_len, "PORT %s\r\n", pv->unparsed);

	control_send(dst, s_port);
	if(control_recv(dst) != 200) {
		return false;
	}

	free(s_port);
	char *new_spath = path_append_file(src_dir, filename);
	char *new_dpath = path_append_file(dst_dir, filename);

	s_len = strlen(new_dpath) + 9;
	char *s_stor = malloc(s_len);
	snprintf(s_stor, s_len, "STOR %s\r\n", new_dpath);

	control_send(dst, s_stor);
	if(control_recv(dst) != 150) {
		free(new_spath);
		free(new_dpath);
		return false;
	}

	free(s_stor);

	s_len = strlen(new_spath) + 9;
	char *s_retr = malloc(s_len);
	snprintf(s_retr, s_len, "RETR %s\r\n", new_spath);

	//also mark dst as busy
	site_busy(dst);

	gettimeofday(&time_start, NULL);

	control_send(src, s_retr);

	if(control_recv(src) != 150) {
		//if retr fails, cancel the STOR with pasv trick
		control_send(src, "PASV\r\n");
		control_recv(src);

		//wait for STOR resp
		control_recv(dst);
		free(new_spath);
		free(new_dpath);
		return false;
	}

	if(control_recv(src) != 226) {
		free(new_spath);
		free(new_dpath);
		return false;
	}

	if(control_recv(dst) != 226) {
		free(new_spath);
		free(new_dpath);
		return false;
	}

	free(new_spath);
	free(new_dpath);

	gettimeofday(&time_end, NULL);
	char *speed = s_calc_transfer_speed(&time_start, &time_end, file->size);
	log_ui(src->thread_id, LOG_T_I, "%s fxp'd at %s in %ds\n", filename, speed, time_end.tv_sec - time_start.tv_sec);	

	return true;
}

bool fxp_recursive(struct site_info *src, struct site_info *dst, char *dirname, char *src_dir, char *dst_dir) {
	struct file_item *file = find_file(src->cur_dirlist, dirname);

	if(file == NULL) {
		return false;
	}

	if(file->skip) {
		log_ui(src->thread_id, LOG_T_W, "%s: skip\n", dirname);
		return true;
	}

	char *new_spath = path_append_dir(src_dir, dirname);
	char *new_dpath = path_append_dir(dst_dir, dirname);

	struct file_item *rfile = find_file(dst->cur_dirlist, dirname);

	if(rfile == NULL) {
		if(!ftp_mkd(dst, new_dpath)) {
			log_ui(dst->thread_id, LOG_T_E, "%s: mkdir failed!\n", dirname);
			free(new_spath);
			free(new_dpath);
			return false;
		}
	} else if(rfile->file_type != FILE_TYPE_DIR) {
		log_ui(dst->thread_id, LOG_T_E, "%s: remote file not a directory!\n", dirname);
		free(new_spath);
		free(new_dpath);
		return false;	
	}

	//cwd and list src
	if(!ftp_cwd(src, dirname)) {
		free(new_spath);
		free(new_dpath);
		return false;
	}

	if(!ftp_ls(src)) {
		ftp_cwd_up(src);
		free(new_spath);
		free(new_dpath);
		return false;
	}

	//cwd and list dst
	if(!ftp_cwd(dst, dirname)) {
		ftp_cwd_up(src);
		free(new_spath);
		free(new_dpath);
		return false;
	}

	if(!ftp_ls(dst)) {
		ftp_cwd_up(src);
		ftp_cwd_up(dst);
		free(new_spath);
		free(new_dpath);
		return false;
	}

	struct file_item *fl = src->cur_dirlist;

	if(fl == NULL) {
		log_ui(src->thread_id, LOG_T_W, "%s: empty dir\n", dirname);
		free(new_spath);
		free(new_dpath);
		return true;
	}

	bool flag_failed = false;

	while(fl != NULL) {
		if(fl->file_type == FILE_TYPE_FILE) {
			log_ui(src->thread_id, LOG_T_I, "%s: fxp'ing file..\n", fl->file_name);
			if(!fxp(src, dst, fl->file_name, new_spath, new_dpath)) {
				log_ui(src->thread_id, LOG_T_E, "%s: fxp failed!\n", fl->file_name);
				flag_failed = true;
			}
		} else if(fl->file_type == FILE_TYPE_DIR) {
			log_ui(src->thread_id, LOG_T_I, "%s: fxp'ing dir..\n", fl->file_name);
			if(!fxp_recursive(src, dst, fl->file_name, new_spath, new_dpath)) {
				log_ui(src->thread_id, LOG_T_E, "%s/: fxp failed!\n", fl->file_name);
				flag_failed = true;
			}
		}

		fl = fl->next;
	}

	free(new_spath);
	free(new_dpath);
	return ftp_cwd_up(src) && ftp_cwd_up(dst) && !flag_failed;
}
