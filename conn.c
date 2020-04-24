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
	uint32_t len = strlen(data);
	uint32_t n_sent = 0;
	uint32_t n = 0;

	log_w("%s",data);

	while(n_sent < len) {
		n = write_control_socket(site, data+n_sent, len-n_sent, force_plaintext);
		n_sent += n;

		if(n == -1) {
			return false;
		}
	}

	return true;
}

int32_t __control_recv(struct site_info *site, bool force_plaintext) {
	char buf[CONTROL_BUF_SZ];
	char line[CONTROL_LINE_SZ]; // control code + space/dash

	uint32_t code, col_count = 0;
	int32_t numbytes;
	int32_t out_sz = CONTROL_INT_BUF_CHUNK_SZ;
	int32_t out_cur_ofs = 0;

	if(site->last_recv != NULL) {
		//free old received data
		free(site->last_recv);
	}

	//initial size 4k
	site->last_recv = malloc(out_sz);

	while((numbytes = read_control_socket(site, buf, CONTROL_BUF_SZ-1, force_plaintext)) != 0) {
		if (numbytes == -1) {
			log_w("error recv\n");
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
					site->last_recv[out_cur_ofs+1] = '\0';
					return code;
				}

				continue;
			}

			col_count++;
		}
	}

	site->last_recv[out_cur_ofs+1] = '\0';
	return 0;
}

bool control_send(struct site_info *site, char *data) {
	return __control_send(site, data, false);
}

int32_t control_recv(struct site_info *site) {
	return __control_recv(site, false);
}

bool auth(struct site_info *site) {
	int32_t code;
	char s_auth[SITE_USER_MAX+7];
	char s_pass[SITE_PASS_MAX+7];

	//init TLS connection if enabled
	if(site->use_tls) {
		__control_send(site, "AUTH TLS\n", true);
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
	snprintf(s_auth, SITE_USER_MAX+7, "USER %s\n", site->username);
	control_send(site, s_auth);
	code = control_recv(site);
	
	//username failed?
	if(code != 331) {
		ftp_disconnect(site);
		return false;
	}

	//send password
	snprintf(s_pass, SITE_PASS_MAX+7, "PASS %s\n", site->password);
	control_send(site, s_pass);
	code = control_recv(site);

	//password fail?
	if(code != 230) {
		ftp_disconnect(site);
		return false;
	}

	//set protection buffer to 0
	control_send(site, "PBSZ 0\n");
	code = control_recv(site);

	//set binary mode
	control_send(site, "TYPE I\n");
	code = control_recv(site);

	//get current working dir
	control_send(site, "PWD\n");
	code = control_recv(site);

	//didnt get correct pwd reply, indicate bad login
	if(code != 257) {
		ftp_disconnect(site);
		return false;
	}

	char *pwd = parse_pwd(site->last_recv);

	if(pwd == NULL) {
		log_w("error parsing pwd\n");
		ftp_disconnect(site);
		return false;
	}

	site_set_cwd(site, pwd);

	log_w("pwd set to %s\n", site->current_working_dir);

	//get initial filelist
	control_send(site, "STAT -la\n");
	code = control_recv(site);

//	control_send(site, "CWD /\n");
//	code = control_recv(site);

	return true;
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


	//do auth
	return auth(site);
}

void ftp_disconnect(struct site_info *site) {
	close(site->socket_fd);
	log_w("FTP session was terminated\n");
}

bool ftp_prot(struct site_info *site) {
	control_send(site, "PROT P\n");

	if(control_recv(site) == 200) {
		site->prot_sent = true;
		return true;
	}

	return false;
}

struct pasv_details *ftp_pasv(struct site_info *site, bool handshake) {
	if(handshake) {
		control_send(site, "CPSV\n");
	} else {
		control_send(site, "PASV\n");
	}
	
	if(control_recv(site) != 227) {
		return NULL;
	}

	struct pasv_details *pv = malloc(sizeof(struct pasv_details));

	if(!parse_pasv(site->last_recv, pv->host, &pv->port)) {
		return NULL;
	}

	return pv;
}

bool ftp_retr(struct site_info *site, char *filename) {
	int slen = strlen(filename)+8;
	char *s_retr = malloc(slen);

	snprintf(s_retr, slen, "RETR %s\n", filename);
	control_send(site, s_retr);

	free(s_retr);

	return control_recv(site) == 150;	
}

bool ftp_stor(struct site_info *site, char *filename) {
	int slen = strlen(filename)+8;
	char *s_retr = malloc(slen);

	snprintf(s_retr, slen, "STOR %s\n", filename);
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

bool ftp_get(struct site_info *site, char *filename, char *local_dir) {
	if(!ftp_open_data(site)) {
		return false;
	}

	if(!ftp_retr(site, filename)) {
		close(site->data_socket_fd);
		return false;
	}

	if(site->use_tls) {
		if(!ftp_data_enable_tls(site)) {
			return false;
		}
	}

	//create local path
	int new_len = strlen(local_dir)+strlen(filename)+2;
	char *new_lpath = malloc(new_len);
	strlcpy(new_lpath, local_dir, new_len);
	strlcat(new_lpath, filename, new_len);

	char buf[DATA_BUF_SZ];
	int32_t numbytes;
	FILE *fd = fopen(new_lpath, "wb");
	
	if(fd == NULL) {
		log_w("error opening file for writing\n");
		close(site->data_socket_fd);
		return false;
	}

	bool write_failed = false;

	while((numbytes = read_data_socket(site, buf, DATA_BUF_SZ-1, false)) != 0) {
		if (numbytes == -1) {
			log_w("error recv\n");
			write_failed = true;
			break;
		}
		
		if(fwrite(buf, sizeof(char), numbytes, fd) != numbytes) {
			write_failed = true;
			break;
		}
	}

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

	return code == 226;
}

bool ftp_put(struct site_info *site, char *filename, char *local_dir) {
	if(!ftp_open_data(site)) {
		return false;
	}

	if(!ftp_stor(site, filename)) {
		close(site->data_socket_fd);
		return false;
	}

	if(site->use_tls) {
		if(!ftp_data_enable_tls(site)) {
			return false;
		}
	}

	//create local path
	int new_len = strlen(local_dir)+strlen(filename)+2;
	char *new_lpath = malloc(new_len);
	strlcpy(new_lpath, local_dir, new_len);
	strlcat(new_lpath, filename, new_len);

	char buf[DATA_BUF_SZ];
	FILE *fd = fopen(new_lpath, "rb");
	free(new_lpath);
	uint32_t n_sent = 0;
	uint32_t n = 0;
	size_t n_read = 0;
	//long int len;
	
	if(fd == NULL) {
		log_w("error opening file for reading\n");
		close(site->data_socket_fd);
		return false;
	}

	//fseek(fd, 0L, SEEK_END);
	//len = ftell(fd);
	//rewind(fd);

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

			if(n == -1) {
				log_w("error writing data\n");
				fclose(fd);
				close(site->data_socket_fd);
				return false;
			}
		}
	}

	if(site->use_tls) {
		//important to make sure the ssl shutdown has finished before closing sock
		while(SSL_shutdown(site->data_secure_fd) == 0) {
		}
		SSL_free(site->data_secure_fd);
	}

	fclose(fd);
	close(site->data_socket_fd);
	return control_recv(site) == 226;
}

bool ftp_ls(struct site_info *site) {
	control_send(site, "STAT -la\n");

	if(control_recv(site) != 213) {
		return false;
	}

	struct file_item *fl = parse_list(site->last_recv);

	//if(fl == NULL) {
	//	return false;
	//}

	site->cur_dirlist = fl;
	return true;
}

bool ftp_cwd(struct site_info *site, char *dirname) {
	int cmd_len = strlen(dirname) + 6;
	char *s_cmd = malloc(cmd_len);

	snprintf(s_cmd, cmd_len, "CWD %s\n", dirname);

	control_send(site, s_cmd);
	free(s_cmd);

	return control_recv(site) == 250;

}

bool ftp_mkd(struct site_info *site, char *dirname) {
	int cmd_len = strlen(dirname) + 6;
	char *s_cmd = malloc(cmd_len);

	snprintf(s_cmd, cmd_len, "MKD %s\n", dirname);

	control_send(site, s_cmd);
	free(s_cmd);

	return control_recv(site) == 257;
}

bool ftp_get_recursive(struct site_info *site, char *dirname, char *local_dir) {
	//cd into dir
	if(!ftp_cwd(site, dirname)) {
		return false;
	}

	int new_len = strlen(local_dir)+strlen(dirname)+2;
	char *new_lpath = malloc(new_len);
	strlcpy(new_lpath, local_dir, new_len);
	strlcat(new_lpath, dirname, new_len);
	strlcat(new_lpath, "/", new_len);
	//printf("new: %s\n", new_lpath);

	if(!file_exists(new_lpath)) {
		log_ui(site->thread_id, LOG_T_I, "%s: creating dir\n", new_lpath);
		if(mkdir(new_lpath, 0755) != 0) {
			log_ui(site->thread_id, LOG_T_E, "%s: unable to mkdir!\n", new_lpath);
			ftp_cwd(site, "..");
			ftp_ls(site);
			free(new_lpath);
			return false;
		}
	}

	if(!ftp_ls(site)) {
		log_ui(site->thread_id, LOG_T_E, "%s: unable to get dirlist!\n", dirname);
		return false;
	}

	if(site->cur_dirlist == NULL) {
		log_ui(site->thread_id, LOG_T_W, "%s: empty dir\n", dirname);
		ftp_cwd(site, ".."); // go back
		ftp_ls(site);
		free(new_lpath);
		return true;
	}

	struct file_item *fl = site->cur_dirlist;
	bool flag_failed = false;

	while(fl != NULL) {
		if(fl->file_type == FILE_TYPE_FILE) {
			log_ui(site->thread_id, LOG_T_I, "%s: downloading file..\n", fl->file_name);
			if(!ftp_get(site, fl->file_name, new_lpath)) {
				log_ui(site->thread_id, LOG_T_E, "%s: download failed!\n", fl->file_name);
				flag_failed = true;
			}
		} else if(fl->file_type == FILE_TYPE_DIR) {
			log_ui(site->thread_id, LOG_T_I, "%s: downloading dir..\n", fl->file_name);
			if(!ftp_get_recursive(site, fl->file_name, new_lpath)) {
				log_ui(site->thread_id, LOG_T_E, "%s/: download failed!\n", fl->file_name);
				flag_failed = true;
			}
		}
		fl = fl->next;
	}	
	
	free(new_lpath);
	return ftp_cwd(site, "..") && ftp_ls(site) && !flag_failed;
}

bool ftp_put_recursive(struct site_info *site, char *dirname, char *local_dir) {
	int new_len = strlen(local_dir)+strlen(dirname)+2;
	char *new_lpath = malloc(new_len);
	strlcpy(new_lpath, local_dir, new_len);
	strlcat(new_lpath, dirname, new_len);
	strlcat(new_lpath, "/", new_len);
	//printf("new: %s\n", new_lpath);return false;

	struct file_item *rfile = find_file(site->cur_dirlist, dirname);

	if(rfile == NULL) {
		if(!ftp_mkd(site, dirname)) {
			log_ui(site->thread_id, LOG_T_E, "%s: mkdir failed!\n", dirname);
		}
	} else if(rfile->file_type != FILE_TYPE_DIR) {
		log_ui(site->thread_id, LOG_T_E, "%s: remote file not a directory!\n", dirname);
		return false;
	}

	if(!ftp_cwd(site, dirname)) {
		return false;
	}

	if(!ftp_ls(site)) {
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
			if(!ftp_put(site, lp->file_name, new_lpath)) {
				log_ui(site->thread_id, LOG_T_E, "%s: upload failed!\n", lp->file_name);
				flag_failed = true;
			}
		} else if(lp->file_type == FILE_TYPE_DIR) {
			log_ui(site->thread_id, LOG_T_I, "%s: uploading dir..\n", lp->file_name);
			if(!ftp_put_recursive(site, lp->file_name, new_lpath)) {
				log_ui(site->thread_id, LOG_T_E, "%s/: upload failed!\n", lp->file_name);
				flag_failed = true;
			}
		}

		lp = lp->next;
	}

	free(new_lpath);

	//free dir data
	while(l_list != NULL) {
		lp = l_list;
		l_list = l_list->next;
		free(lp);
	}

	return ftp_cwd(site, "..") && ftp_ls(site) && !flag_failed;
}
