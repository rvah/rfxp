#include "mock_net.h"

static uint32_t opener_call_count = 0;
static uint32_t opener_secure_call_count = 0;
static uint32_t closer_call_count = 0;
static uint32_t sender_call_count = 0;
static uint32_t ssender_call_count = 0;
static uint32_t recv_call_count = 0;
static uint32_t srecv_call_count = 0;

static char **request = NULL;
static char **response = NULL;
static uint32_t request_index = 0;
static uint32_t response_index = 0;

static ssize_t request_socket(const void *buf, size_t n) {
	printf("---CMP S---\n%s\n%s\n---CMP E---\n", (char *)buf, request[request_index]);
	if(strcmp(buf, request[request_index++]) != 0) {
		return -1;
	}

	return n;
}

static ssize_t respond_socket(void *buf, size_t n) {
	if(response[response_index] == NULL) {
//		printf("BAD\n");
		return -1;
	}

	return strlcpy(buf, response[response_index++], n);
}

void mock_net_set_socket_request(char **data) {
	request = data;
}

void mock_net_set_socket_response(char **data) {
	response = data;
}

int32_t mock_net_socket_opener(char *address, char *port) {
	opener_call_count++;
	return 0;
}

int mock_net_socket_secure_opener(SSL *ssl) {
	opener_secure_call_count++;
	return 0;
}

bool mock_net_socket_secure_opener_verify_opened(uint32_t times) {
	return times == opener_secure_call_count;
}

bool mock_net_socket_opener_verify_opened(uint32_t times) {
	return times == opener_call_count;
}

int32_t mock_net_socket_closer(int32_t fd) {
	closer_call_count++;
	return 0;
}

ssize_t mock_net_socket_receiver(int fd, void *buf, size_t n, int flags) {
	recv_call_count++;
	return respond_socket(buf, n);
}

ssize_t mock_net_socket_sender(int fd, const void *buf, size_t n, int flags) {
	sender_call_count++;
	return request_socket(buf, n);
}

int mock_net_socket_secure_receiver(SSL *ssl, void *buf, int num) {
	srecv_call_count++;
	return respond_socket(buf, num);
}

int mock_net_socket_secure_sender(SSL *ssl, const void *buf, int num) {
	ssender_call_count++;
	return request_socket(buf, num);
}

bool mock_net_socket_closer_verify_opened(uint32_t times) {
	return times == closer_call_count;
}

bool mock_net_socket_sender_verify_opened(uint32_t times) {
	return times == sender_call_count;
}

bool mock_net_socket_secure_sender_verify_opened(uint32_t times) {
	return times == ssender_call_count;
}

bool mock_net_socket_receiver_verify_opened(uint32_t times) {
	return times == recv_call_count;
}

bool mock_net_socket_secure_receiver_verify_opened(uint32_t times) {
	return times == srecv_call_count;
}

void mock_net_reset() {
	opener_call_count = 0;
	opener_secure_call_count = 0;
	closer_call_count = 0;
	sender_call_count = 0;
	ssender_call_count = 0;
	recv_call_count = 0;
	srecv_call_count = 0;

	request_index = 0;
	response_index = 0;

	request = NULL;
	response = NULL;
}
