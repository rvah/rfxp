#include "ident.h"

static struct ident_config *ident_conf = NULL;

void ident_set_setting(const char *name, const char *value) {
	if(ident_conf == NULL) {
		ident_conf = malloc(sizeof(struct ident_config));

		ident_conf->enabled = false;
		ident_conf->always_on = false;
		ident_conf->port[0] = '\0';
		ident_conf->name[0] = '\0';
	}

	char *c_value = strdup(value);

	str_trim(c_value);

	if(strcmp(name, "enabled") == 0) {
		ident_conf->enabled = strcmp(c_value, "true") == 0;
	} else if(strcmp(name, "port") == 0) {
		strlcpy(ident_conf->port, c_value, IDENT_MAX_PORT_LEN);
	} else if(strcmp(name, "name") == 0) {
		strlcpy(ident_conf->name, c_value, IDENT_MAX_NAME_LEN);
	} else if(strcmp(name, "always_on") == 0) {
		ident_conf->always_on = strcmp(c_value, "true") == 0;
	}

	free(c_value);
}

void ident_start() {
	if(ident_conf == NULL) {
		log_ui(THREAD_ID_IDENT, LOG_T_E, "Ident: config not set!\n");
		return;
	}

	if(!ident_conf->enabled) {
		log_w("ident not enabled, skipping startup\n");
		return;
	}

	uint32_t sockfd = net_open_server_socket(ident_conf->port);
	uint32_t newfd;
	struct sockaddr_storage client_addr;
	socklen_t sin_size;
	char s[INET6_ADDRSTRLEN];

	if(sockfd == -1) {
		log_ui(THREAD_ID_IDENT, LOG_T_E, "Ident: error opening socket!\n");
		return;
	}

	log_ui(THREAD_ID_IDENT, LOG_T_I, "Ident server is ready.\n");

	while(1) {
		sin_size = sizeof(client_addr);
		newfd = accept(sockfd, (struct sockaddr *)&client_addr, &sin_size);

		if (newfd == -1) {
			log_w("client connect: accept err\n");
			continue;
		}

		inet_ntop(client_addr.ss_family, net_get_in_addr((struct sockaddr *)&client_addr), s, sizeof(s));

		//if always on not enabled, make sure some site is in connecting state
		//- if not, close connection
		if(!ident_conf->always_on) {
			struct site_list *sl = site_get_sites_connecting();
			bool is_connecting = sl != NULL;
			site_destroy_list(sl);

			if(!is_connecting) {
				close(newfd);

				log_ui(THREAD_ID_IDENT, LOG_T_W, "Ident: suspicious connection attempt from %s!\n", s);
			}
		}

		log_w("ident server: got connection from %s\n", s);

		if (!fork()) {
			close(sockfd); // child doesn't need the listener

			char buf[1024];
			char *ident_fmt = "%s : USERID : UNIX : %s\n";
			int l_reply = 1024 + strlen(ident_fmt) + strlen(ident_conf->name) + 1;
			char *s_reply = malloc(l_reply);

			ssize_t n = recv(newfd, buf, 1024, 0);

			int l_buf = strlen(buf);
			bool replied = false;

			if((n != -1) && (l_buf > 0) && (l_buf < 1024)) {
				str_rtrim_special_only(buf);

				if(strlen(buf) > 0) {
					snprintf(s_reply, l_reply, ident_fmt, buf, ident_conf->name);
					replied = true;
				}
			}
			
			if(!replied) {
				log_w("error recv ident request\n");
				s_reply[0] = '\0';
			}
			
			if (send(newfd, s_reply, strlen(s_reply)+1, 0) == -1) {
				log_w("error: ident send");
			} else {
				log_w("replied: %s", s_reply);
			}

			free(s_reply);
			close(newfd);
			exit(0);
		}

		close(newfd);
	}

	close(sockfd);
}
