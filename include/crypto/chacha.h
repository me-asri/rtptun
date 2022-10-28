#ifndef RTPTUN_CRYPTO_CHACHA_H
#define RTPTUN_CRYPTO_CHACHA_H

#include <stddef.h>

#include <sodium/crypto_aead_chacha20poly1305.h>

#define CHACHA_KEY_LEN crypto_aead_chacha20poly1305_ietf_KEYBYTES
#define CHACHA_NONCE_LEN crypto_aead_chacha20poly1305_ietf_NPUBBYTES
#define CHACHA_MAC_LEN crypto_aead_chacha20poly1305_ietf_ABYTES

typedef struct chacha_cipher
{
    unsigned char key[CHACHA_KEY_LEN];
    unsigned char nonce[CHACHA_NONCE_LEN];
} chacha_cipher_t;

char *chacha_gen_key();

int chacha_init(chacha_cipher_t *cipher, const char *key);
int chacha_encrypt(chacha_cipher_t *cipher, const char *data, size_t data_len, char *ciphertext, char *mac, char *nonce);
int chacha_decrypt(chacha_cipher_t *cipher, const char *ciphertext, size_t ciphertext_len, const char *mac, const char *nonce, char *data);

#endif