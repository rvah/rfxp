#pragma once

#include <stdint.h>
#include <openssl/ssl.h>
#include "general.h"
#include "io.h"

void mock_io_set_file_buffer(uint8_t *buf, size_t n);
void mock_io_set_ftp_buffer(uint8_t *buf, size_t n);
ssize_t mock_io_file_reader(struct io_item *src, uint8_t *buf);
ssize_t mock_io_ftp_reader(struct io_item *src, uint8_t *buf);
ssize_t mock_io_file_writer(struct io_item *src, uint8_t *buf, ssize_t n);
ssize_t mock_io_ftp_writer(struct io_item *src, uint8_t *buf, ssize_t n);
bool mock_io_file_reader_verify_opened(uint32_t times);
bool mock_io_ftp_reader_verify_opened(uint32_t times);
bool mock_io_file_writer_verify_opened(uint32_t times);
bool mock_io_ftp_writer_verify_opened(uint32_t times);
void mock_io_reset();
