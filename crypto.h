#pragma once

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/aes.h>
#include <openssl/evp.h>

#include "general.h"

void ssl_init();
void ssl_cleanup();
SSL_CTX *ssl_get_context();

bool aes_init(unsigned char *key_data, int key_data_len, unsigned char *salt);
uint8_t *aes_decrypt(uint8_t *encrypted_buf, int *len_buf);
uint8_t *aes_encrypt(uint8_t *decrypted_buf, int *len_buf);
