#ifndef RTPTUN_PROTO_RTP_H
#define RTPTUN_PROTO_RTP_H

#include <stdint.h>
#include <stddef.h>

#include <sys/types.h>

#include <ev.h>

#include "ext/uthash.h"

#include "proto/udp.h"
#include "crypto/chacha.h"

#define RTP_MAX_PAYLOAD_SIZE (UDP_BUFFER_SIZE - sizeof(rtphdr_t) - CHACHA_NONCE_LEN - CHACHA_MAC_LEN)

#define RTP_TIMESTAMP_INCREMENT 3000 // 90kHz / 30FPS video
#define RTP_PAYLOAD_TYPE 97          // Dynamic

typedef uint32_t ssrc_t;

typedef struct rtphdr
{
    uint8_t csrc_count : 4;
    uint8_t extension : 1;
    uint8_t padding : 1;
    uint8_t version : 2;
    uint8_t payload_type : 7;
    uint8_t marker : 1;
    uint16_t seq_number;
    uint32_t timestamp;
    ssrc_t ssrc;
} rtphdr_t;

typedef struct rtp_dest
{
    ssrc_t ssrc;

    struct sockaddr_storage addr;
    socklen_t addr_len;

    uint32_t timestamp;
    uint8_t pl_type;

    UT_hash_handle hh;
} rtp_dest_t;

typedef struct rtp_socket rtp_socket_t;
typedef void (*rtp_recv_callback_t)(rtp_socket_t *socket, unsigned char *data, ssize_t data_len, ssrc_t ssrc);
typedef void (*rtp_send_callback_t)(rtp_socket_t *socket, ssize_t sent);

typedef struct rtp_socket
{
    udp_socket_t *udp_sock;
    int connected;

    chacha_cipher_t cipher;

    rtp_recv_callback_t recv_cb;
    rtp_send_callback_t send_cb;
    void *user_data;

    unsigned int rand_seed;

    uint16_t seq_num;

    struct rtp_dest *rtp_dest_map;
} rtp_socket_t;

rtp_socket_t *rtp_connect(struct ev_loop *loop, const char *address, const char *port, const char *key,
                          rtp_recv_callback_t recv_callback, rtp_send_callback_t send_callback, void *user_data);
rtp_socket_t *rtp_listen(struct ev_loop *loop, const char *address, const char *port, const char *key,
                         rtp_recv_callback_t recv_callback, rtp_send_callback_t send_callback, void *user_data);
void rtp_destroy(rtp_socket_t *socket);

int rtp_send(rtp_socket_t *socket, const unsigned char *data, size_t data_len, ssrc_t ssrc);

int rtp_close_stream(rtp_socket_t *socket, ssrc_t ssrc);

ssrc_t rtp_random_ssrc(rtp_socket_t *socket);

#endif