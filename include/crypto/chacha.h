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

int chacha_init(chacha_cipher_t *cipher, const char *b64_key);
int chacha_encrypt(chacha_cipher_t *cipher, const unsigned char *data, size_t data_len,
                   unsigned char *ciphertext, unsigned char mac[CHACHA_MAC_LEN], unsigned char nonce[CHACHA_NONCE_LEN]);
int chacha_decrypt(chacha_cipher_t *cipher, const unsigned char *ciphertext, size_t ciphertext_len,
                   const unsigned char mac[CHACHA_MAC_LEN], const unsigned char nonce[CHACHA_NONCE_LEN], unsigned char *data);

#endif