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

ssize_t read_socket(struct site_info *site, void *buf, size_t len, bool force_plaintext) {
	if(site->use_tls && !force_plaintext) {
		return SSL_read(site->secure_fd, buf, len);
	} else {
		return recv(site->socket_fd, buf, len, 0);
	}
}

ssize_t write_socket(struct site_info *site, const void *buf, size_t len, bool force_plaintext) {
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

	log_w(data);

	while(n_sent < len) {
		n = write_socket(site, data+n_sent, len-n_sent, force_plaintext);
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

	while((numbytes = read_socket(site, buf, CONTROL_BUF_SZ-1, force_plaintext)) != 0) {
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
				log_w(line);

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

bool ftp_connect(struct site_info *site) {
	int32_t sockfd;
	struct addrinfo hints, *servinfo, *p;
	int32_t rv;
	char s[INET6_ADDRSTRLEN];

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(site->address, site->port, &hints, &servinfo)) != 0) {
		log_w("getaddrinfo: %s\n", gai_strerror(rv));
		return false;
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
		log_w("client: failed to connect\n");
		return false;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
	log_w("client: connecting to %s\n", s);

	freeaddrinfo(servinfo); // all done with this structure

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
