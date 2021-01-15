#include "io.h"
#include <bits/stdint-uintn.h>

static ssize_t default_file_reader(struct io_item *src, uint8_t *buf) {
	if(src->local_fd == NULL) {
		src->local_fd = fopen(src->path, "rb");
	}

	if(src->local_fd == NULL) {
		log_w("error opening file for reading\n");
		return -1;
	}

	size_t n_read = fread(buf, 1, IO_BUF_SIZE, src->local_fd);

	if(n_read != IO_BUF_SIZE && ferror(src->local_fd) != 0) {
		log_w("error reading data\n");
		return -1;
	}

	return n_read;
}

static ssize_t (*file_reader)(
		struct io_item *, uint8_t *) = default_file_reader;

static ssize_t default_ftp_reader(struct io_item *src, uint8_t *buf) {
	return data_read_socket(src->site, buf, IO_BUF_SIZE, false);
}

static ssize_t (*ftp_reader)(struct io_item *, uint8_t *) = default_ftp_reader;

static ssize_t default_file_writer(
		struct io_item *src, uint8_t *buf, ssize_t n) {
	if(src->local_fd == NULL)
		src->local_fd = fopen(src->path, "wb");

	if(src->local_fd == NULL) {
		log_w("error opening file for writing\n");
		return -1;
	}

	if(fwrite(buf, sizeof(uint8_t), n, src->local_fd) != n) {
		log_w("error writing data\n");
		return -1;
	}

	return n;
}

static ssize_t (*file_writer)(
		struct io_item *, uint8_t *, ssize_t) = default_file_writer;

static ssize_t default_ftp_writer(
		struct io_item *src, uint8_t *buf, ssize_t n) {
	ssize_t n_sent = 0;
	ssize_t n_sent_tot = 0;

	while(n_sent_tot < n) {
		n_sent = data_write_socket(src->site, buf+n_sent_tot, n-n_sent_tot,
				false);
		n_sent_tot += n_sent;

		if(n_sent == -1) {
			log_w("error writing data\n");
			return -1;
		}
	}
	return n_sent_tot;
}

static ssize_t (*ftp_writer)(
		struct io_item *, uint8_t *, ssize_t n) = default_ftp_writer;

static void close_local(struct io_item *item) {
	if(item->local_fd != NULL) {
		fclose(item->local_fd);
	}
}

static void close_remote(struct io_item *item) {
	if(item->site == NULL) {
		return;
	}

	if(item->site->use_tls) {
		//important to make sure the ssl shutdown has finished before closing
		while(SSL_shutdown(item->site->data_secure_fd) == 0) {
		}
		SSL_free(item->site->data_secure_fd);
	}

	net_close_socket(item->site->data_socket_fd);
}

static void io_close(struct io_item *item) {
	if(item->type == io_type_LOCAL) {
		close_local(item);
	} else {
		close_remote(item);
	}
}

void io_set_file_reader(ssize_t (*r)(struct io_item *, uint8_t *)) {
	file_reader = r;
}

void io_set_ftp_reader(ssize_t (*r)(struct io_item *, uint8_t *)) {
	ftp_reader = r;
}

void io_set_file_writer(ssize_t (*w)(struct io_item *, uint8_t *, ssize_t)) {
	file_writer = w;
}

void io_set_ftp_writer(ssize_t (*w)(struct io_item *, uint8_t *, ssize_t)) {
	ftp_writer = w;
}

ssize_t io_transfer_data(struct io_item *src, struct io_item *dst,
		void (*update)(void *, size_t), void *update_arg) {
	ssize_t (*reader)(struct io_item *, uint8_t *);
	ssize_t (*writer)(struct io_item *, uint8_t *, ssize_t);

	if(src->type == io_type_LOCAL) {
		reader = file_reader;
	} else {
		reader = ftp_reader;
	}

	if(dst->type == io_type_LOCAL) {
		writer = file_writer;
	} else {
		writer = ftp_writer;
	}

	size_t read_n;
	size_t w_total = 0;
	uint8_t buf[IO_BUF_SIZE];

	while((read_n = reader(src, buf)) != 0) {
		if(read_n == -1) {
			log_w("error: could not read data source\n");
			w_total = -1;
			break;
		}

		if(writer(dst, buf, read_n) == -1) {
			log_w("error: could not write data destination\n");
			w_total = -1;
			break;
		}

		w_total += read_n;

		update(update_arg, read_n);
	}

	io_close(src);
	io_close(dst);

	return w_total;
}

struct io_item *io_item_create_local(const char *path) {
	struct io_item *r = malloc(sizeof(struct io_item));
	r->type = io_type_LOCAL;
	r->local_fd = NULL;
	r->path = strdup(path);

	return r;
}

struct io_item *io_item_create_remote(struct site_info *site) {
	struct io_item *r = malloc(sizeof(struct io_item));
	r->type = io_type_REMOTE;
	r->site = site;
	r->path = NULL;

	return r;
}

void io_item_destroy(struct io_item *item) {
	if(item == NULL) {
		return;
	}

	free(item->path);
	free(item);
}
