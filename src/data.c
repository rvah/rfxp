#include "data.h"

ssize_t data_read_socket(struct site_info *site, void *buf, size_t len, bool force_plaintext) {
	if(site->use_tls && !force_plaintext) {
		return SSL_read(site->data_secure_fd, buf, len);
	} else {
		return recv(site->data_socket_fd, buf, len, 0);
	}
}

ssize_t data_write_socket(struct site_info *site, const void *buf, size_t len, bool force_plaintext) {
	if(site->use_tls && !force_plaintext) {
		return SSL_write(site->data_secure_fd, buf, len);
	} else {
		return send(site->data_socket_fd, buf, len, 0);
	}
}
