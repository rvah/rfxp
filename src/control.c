#include "control.h"
#include "net.h"

/*
 * ----------------
 *
 * Private functions
 *
 * ----------------
 */
static uint32_t parse_code(char *buf) {
	char a = buf[0] - 0x30;
	char b = buf[1] - 0x30;
	char c = buf[2] - 0x30;

	//check if numbers between 0 to 9, if not return with error
	if(((a > 9) || (a < 0)) || ((b > 9) || (b < 0)) || ((c > 9) || (c < 0))) {
		return -1;
	}

	return a*100 + b*10 + c;
}

static bool response_is_eof(char *buf) {
	return buf[3] == ' ';
}

static ssize_t read_control_socket(struct site_info *site, void *buf, size_t len, bool force_plaintext) {
	if(site->use_tls && !force_plaintext) {
		return net_socket_secure_recv(site->secure_fd, buf, len);
	} else {
		return net_socket_recv(site->socket_fd, buf, len, 0);
	}
}

static ssize_t write_control_socket(struct site_info *site, const void *buf, size_t len, bool force_plaintext) {
	if(site->use_tls && !force_plaintext) {
		return net_socket_secure_send(site->secure_fd, buf, len);
	} else {
		return net_socket_send(site->socket_fd, buf, len, 0);
	}
}

static bool send_request(struct site_info *site, char *data, bool force_plaintext) {
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

static int32_t recv_response(struct site_info *site, bool force_plaintext) {
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
				code = parse_code(line);

				col_count = 0;

				if((code != -1) && response_is_eof(line)) {
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

/*
 * ----------------
 *
 * Public functions
 *
 * ----------------
 */

bool control_send(struct site_info *site, char *data) {
	return send_request(site, data, !site->is_secure);
}

int32_t control_recv(struct site_info *site) {
	int32_t code = recv_response(site, !site->is_secure);

	//connection timed out, kill socket
	if(code == 421) {
		cmd_execute(site->thread_id, EV_SITE_CLOSE, NULL);

		struct msg *m = malloc(sizeof(struct msg));
		m->to_id = THREAD_ID_UI;
		m->from_id = site->thread_id;
		m->event = EV_UI_RM_SITE;
		msg_send(m);

		log_ui_e("%s: connection timed out.\n", site->name);
	}

	return code;
}
