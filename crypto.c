#include "crypto.h"

EVP_CIPHER_CTX *__evp_en;
EVP_CIPHER_CTX *__evp_de;

void ssl_init() {
	SSL_library_init();
	OpenSSL_add_all_algorithms();
	SSL_load_error_strings();
}

void ssl_cleanup() {
	EVP_cleanup();
	EVP_CIPHER_CTX_cleanup(__evp_en);
	EVP_CIPHER_CTX_cleanup(__evp_de);
}

SSL_CTX *ssl_get_context() {
	const SSL_METHOD *method;
	SSL_CTX *ctx;
	method = TLS_client_method();
	ctx = SSL_CTX_new(method);
	if ( ctx == NULL ) {
		ERR_print_errors_fp(stderr);
		return NULL;
	}
	return ctx;
}

void display_cert(SSL* ssl) {
	X509 *cert;
	char *line;

	cert = SSL_get_peer_certificate(ssl);

	if ( cert != NULL ) {
		printf("Server certificates:\n");
		line = X509_NAME_oneline(X509_get_subject_name(cert), 0, 0);
		printf("Subject: %s\n", line);
		free(line);
		line = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0);
		printf("Issuer: %s\n", line);
		free(line);
		X509_free(cert);
	
		return;
	}

	printf("Info: No client certificates configured.\n");
}

bool aes_init(unsigned char *key_data, int key_data_len, unsigned char *salt) {
  int i, nrounds = 5;
  unsigned char key[32], iv[32];
  
  /*
   * Gen key & IV for AES 256 CBC mode. A SHA1 digest is used to hash the supplied key material.
   * nrounds is the number of times the we hash the material. More rounds are more secure but
   * slower.
   */
  i = EVP_BytesToKey(EVP_aes_256_cbc(), EVP_sha1(), salt, key_data, key_data_len, nrounds, key, iv);
  if (i != 32) {
    printf("Key size is %d bits - should be 256 bits\n", i);
    return false;
  }

  //EVP_CIPHER_CTX_init(e_ctx);
  __evp_en = EVP_CIPHER_CTX_new();
  EVP_EncryptInit_ex(__evp_en, EVP_aes_256_cbc(), NULL, key, iv);
  __evp_de = EVP_CIPHER_CTX_new();
  EVP_DecryptInit_ex(__evp_de, EVP_aes_256_cbc(), NULL, key, iv);

  return true;
}

uint8_t *aes_decrypt(uint8_t *encrypted_buf, int *len_buf) {
  int p_len = *len_buf, f_len = 0;
  uint8_t *decrypted_buf = malloc(p_len);
  
  EVP_DecryptInit_ex(__evp_de, NULL, NULL, NULL, NULL);
  EVP_DecryptUpdate(__evp_de, decrypted_buf, &p_len, encrypted_buf, *len_buf);
  EVP_DecryptFinal_ex(__evp_de, decrypted_buf+p_len, &f_len);

  *len_buf = p_len + f_len;
  return decrypted_buf;
}


uint8_t *aes_encrypt(uint8_t *decrypted_buf, int *len_buf) {
  int c_len = *len_buf + AES_BLOCK_SIZE, f_len = 0;
  uint8_t *encrypted_buf = malloc(c_len);

  EVP_EncryptInit_ex(__evp_en, NULL, NULL, NULL, NULL);
  EVP_EncryptUpdate(__evp_en, encrypted_buf, &c_len, decrypted_buf, *len_buf);
  EVP_EncryptFinal_ex(__evp_en, encrypted_buf+c_len, &f_len);

  *len_buf = c_len + f_len;
  return encrypted_buf;
}
