#include "crypto/chacha.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <sodium.h>

chacha_ret_t chacha_gen_key(char **buf, size_t *len)
{
    if (sodium_init() == -1)
        return CHACHA_RET_INITERR;

    unsigned char bin_key[CHACHA_KEY_LEN];
    crypto_aead_chacha20poly1305_ietf_keygen(bin_key);

    unsigned long long max_len = sodium_base64_ENCODED_LEN(CHACHA_KEY_LEN, sodium_base64_VARIANT_ORIGINAL);
    if (!*buf || *len < max_len)
    {
        char *new_buf = realloc(*buf, max_len);
        if (!new_buf)
            return CHACHA_RET_MEMERR;
        *buf = new_buf;
    }

    sodium_bin2base64(*buf, max_len, bin_key, sizeof(bin_key), sodium_base64_VARIANT_ORIGINAL);
    return CHACHA_RET_SUCCESS;
}

chacha_ret_t chacha_init(chacha_cipher_t *cipher, const char *b64_key)
{
    size_t bin_len = 0;

    if (sodium_init() == -1)
        return CHACHA_RET_INITERR;

    if (!b64_key || sodium_base642bin(cipher->key, sizeof(cipher->key), b64_key, strlen(b64_key), NULL, &bin_len, NULL, sodium_base64_VARIANT_ORIGINAL) != 0 || bin_len != sizeof(cipher->key))
        return CHACHA_RET_KEYERR;

    randombytes_buf(cipher->nonce, sizeof(cipher->nonce));

    return CHACHA_RET_SUCCESS;
}

chacha_ret_t chacha_encrypt(chacha_cipher_t *cipher, const unsigned char *data, size_t data_len, unsigned char *ciphertext, unsigned char mac[CHACHA_MAC_LEN], unsigned char nonce[CHACHA_NONCE_LEN])
{
    unsigned long long mac_len;
    if (crypto_aead_chacha20poly1305_ietf_encrypt_detached(ciphertext, mac, &mac_len, data, data_len,
                                                           NULL, 0, NULL, cipher->nonce, cipher->key) == -1)
        return CHACHA_RET_ENCERR;
    memcpy(nonce, cipher->nonce, sizeof(cipher->nonce));

    sodium_increment(cipher->nonce, sizeof(cipher->nonce));

    return CHACHA_RET_SUCCESS;
}

chacha_ret_t chacha_decrypt(chacha_cipher_t *cipher, const unsigned char *ciphertext, size_t ciphertext_len, const unsigned char mac[CHACHA_MAC_LEN], const unsigned char nonce[CHACHA_NONCE_LEN], unsigned char *data)
{
    if (crypto_aead_chacha20poly1305_ietf_decrypt_detached(data, NULL, ciphertext, ciphertext_len,
                                                           mac, NULL, 0, nonce, cipher->key) == -1)
        return CHACHA_RET_ENCERR;

    return CHACHA_RET_SUCCESS;
}
