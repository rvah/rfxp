#pragma once

#include "general.h"
#include "site.h"

#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>

ssize_t data_read_socket(struct site_info *site, void *buf, size_t len, bool force_plaintext);
ssize_t data_write_socket(struct site_info *site, const void *buf, size_t len, bool force_plaintext);
