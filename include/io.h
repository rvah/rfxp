#pragma once

#include "general.h"
#include "log.h"
#include "data.h"
#include "net.h"

#define IO_BUF_SIZE 4096

enum io_type { io_type_LOCAL, io_type_REMOTE };

struct io_item {
	enum io_type type;
	char *path;
	FILE *local_fd;
	struct site_info *site;
};

void io_set_file_reader(ssize_t (*r)(struct io_item *, uint8_t *));
void io_set_ftp_reader(ssize_t (*r)(struct io_item *, uint8_t *));
void io_set_file_writer(ssize_t (*w)(struct io_item *, uint8_t *, ssize_t));
void io_set_ftp_writer(ssize_t (*w)(struct io_item *, uint8_t *, ssize_t));
ssize_t io_transfer_data(struct io_item *src, struct io_item *dst,
		void (*update)(void *, size_t), void *update_arg);
struct io_item *io_item_create_local(const char *path);
struct io_item *io_item_create_remote(struct site_info *site);
void io_item_destroy(struct io_item *item);
