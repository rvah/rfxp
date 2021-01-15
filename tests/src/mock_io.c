#include "mock_io.h"
#include "io.h"

static uint32_t file_reader_call_count = 0;
static uint32_t ftp_reader_call_count = 0;
static uint32_t file_writer_call_count = 0;
static uint32_t ftp_writer_call_count = 0;

static uint8_t *file_buffer = NULL;
static uint8_t *ftp_buffer = NULL;

static size_t file_buffer_len = 0;
static size_t ftp_buffer_len = 0;

static size_t file_buffer_i = 0;
static size_t ftp_buffer_i = 0;

void mock_io_set_file_buffer(uint8_t *buf, size_t n) {
	file_buffer = buf;
	file_buffer_len = n;
	file_buffer_i = 0;
}

void mock_io_set_ftp_buffer(uint8_t *buf, size_t n) {
	ftp_buffer = buf;
	ftp_buffer_len = n;
	ftp_buffer_i = 0;
}

ssize_t mock_io_file_reader(struct io_item *src, uint8_t *buf) {
	ssize_t read_n = 0;

	if(file_buffer == NULL) {
		return -1;
	}

	for(read_n = 0;
			read_n < IO_BUF_SIZE && file_buffer_i < file_buffer_len;
			read_n++, file_buffer_i++)
		buf[read_n] = file_buffer[file_buffer_i];

	return read_n;
}

ssize_t mock_io_ftp_reader(struct io_item *src, uint8_t *buf) {
	ssize_t read_n = 0;

	if(ftp_buffer == NULL) {
		return -1;
	}

	for(read_n = 0;
			read_n < IO_BUF_SIZE && ftp_buffer_i < ftp_buffer_len;
			read_n++, ftp_buffer_i++)
		buf[read_n] = ftp_buffer[ftp_buffer_i];

	return read_n;
}

ssize_t mock_io_file_writer(struct io_item *src, uint8_t *buf, ssize_t n) {
	ssize_t written_n = 0;

	for(written_n = 0; written_n < n; written_n++, file_buffer_i++) {
		if(written_n >= file_buffer_len) {
			return -1;
		}

		file_buffer[file_buffer_i] = buf[written_n];
	}

	return written_n;
}

ssize_t mock_io_ftp_writer(struct io_item *src, uint8_t *buf, ssize_t n) {
	ssize_t written_n = 0;

	for(written_n = 0; written_n < n; written_n++, ftp_buffer_i++) {
		if(written_n >= ftp_buffer_len) {
			return -1;
		}

		ftp_buffer[ftp_buffer_i] = buf[written_n];
	}

	return written_n;
}

bool mock_io_file_reader_verify_opened(uint32_t times) {
	return times == file_reader_call_count;
}

bool mock_io_ftp_reader_verify_opened(uint32_t times) {
	return times == ftp_reader_call_count;
}

bool mock_io_file_writer_verify_opened(uint32_t times) {
	return times == file_writer_call_count;
}

bool mock_io_ftp_writer_verify_opened(uint32_t times) {
	return times == ftp_writer_call_count;
}

void mock_io_reset() {
	file_reader_call_count = 0;
	ftp_reader_call_count = 0;
	file_writer_call_count = 0;
	ftp_writer_call_count = 0;

	file_buffer = NULL;
	ftp_buffer = NULL;

	file_buffer_len = 0;
	ftp_buffer_len = 0;

	file_buffer_i = 0;
	ftp_buffer_i = 0;
}
