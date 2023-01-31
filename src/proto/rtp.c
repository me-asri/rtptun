#define _POSIX_C_SOURCE 199506L

#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <time.h>
#include <limits.h>

#include <sys/types.h>

#ifdef __MINGW32__
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include <ev.h>

#include <sodium.h>

#include "ext/uthash.h"

#include "log.h"
#include "proto/rtp.h"

static void udp_recv_callback(udp_socket_t *socket, char *data, ssize_t data_len,
                              struct sockaddr_storage *address, socklen_t addrlen);
static void udp_send_callback(udp_socket_t *socket, ssize_t sent);

rtp_socket_t *rtp_connect(struct ev_loop *loop, const char *address, const char *port, const char *key,
                          rtp_recv_callback_t recv_callback, rtp_send_callback_t send_callback, void *user_data)
{
    rtp_socket_t *sock = calloc(1, sizeof(*sock));
    if (!sock)
    {
        elog_error("calloc(rtp_socket_t) failed");
        goto error;
    }

    sock->connected = 1;

    randombytes_buf(&sock->seq_num, sizeof(sock->seq_num));

    if (chacha_init(&sock->cipher, key) != 0)
    {
        log_error("Failed to initialize cipher");
        goto error;
    }

    sock->recv_cb = recv_callback;
    sock->send_cb = send_callback;
    sock->user_data = user_data;

    sock->rtp_dest_map = NULL;

    sock->udp_sock = udp_connect(loop, address, port, udp_recv_callback, udp_send_callback, sock);
    if (!sock->udp_sock)
    {
        log_error("udp_connect(%s:%s) failed", address, port);
        goto error;
    }

    return sock;
error:
    if (sock)
    {
        if (sock->udp_sock)
            udp_free(sock->udp_sock);
        free(sock);
    }

    return NULL;
}

rtp_socket_t *rtp_listen(struct ev_loop *loop, const char *address, const char *port, const char *key,
                         rtp_recv_callback_t recv_callback, rtp_send_callback_t send_callback, void *user_data)
{
    rtp_socket_t *sock = calloc(1, sizeof(*sock));
    if (!sock)
    {
        elog_error("calloc(rtp_socket_t) failed");
        goto error;
    }

    sock->connected = 0;
    randombytes_buf(&sock->seq_num, sizeof(sock->seq_num));

    if (chacha_init(&sock->cipher, key) != 0)
    {
        log_error("Failed to initialize cipher");
        goto error;
    }

    sock->recv_cb = recv_callback;
    sock->send_cb = send_callback;
    sock->user_data = user_data;

    sock->rtp_dest_map = NULL;

    sock->udp_sock = udp_listen(loop, address, port, udp_recv_callback, udp_send_callback, sock);
    if (!sock->udp_sock)
    {
        log_error("udp_listen([%s]:%s) failed", address, port);
        goto error;
    }

    return sock;
error:
    if (sock)
    {
        if (sock->udp_sock)
            udp_free(sock->udp_sock);
        free(sock);
    }

    return NULL;
}

void rtp_free(rtp_socket_t *socket)
{
    udp_free(socket->udp_sock);
    rtp_dest_free(socket);

    free(socket);
}

int rtp_send(rtp_socket_t *socket, const char *data, size_t data_len, ssrc_t ssrc)
{
    if (data_len > RTP_MAX_PAYLOAD_SIZE)
    {
        log_error("Data length exceeds maximum UDP buffer size");
        return -1;
    }

    char buffer[UDP_BUFFER_SIZE];
    rtphdr_t *header = (rtphdr_t *)buffer;

    memset(header, 0, sizeof(*header));
    header->version = 2;
    header->ssrc = htonl(ssrc);
    header->seq_number = htons(socket->seq_num);

    char *payload = &buffer[sizeof(rtphdr_t)];
    if (chacha_encrypt(&socket->cipher, data, data_len,
                       payload,
                       &payload[data_len + CHACHA_NONCE_LEN],
                       &payload[data_len]) != 0)
    {
        log_error("Failed to encrypt data");
        return -1;
    }

    socket->seq_num = (socket->seq_num + 1) % UINT16_MAX;

    size_t total_len = data_len + sizeof(rtphdr_t) + CHACHA_NONCE_LEN + CHACHA_MAC_LEN;

    if (socket->connected)
    {
        rtp_dest_t *dest = rtp_dest_set(socket, ssrc, NULL, 0, RTP_PAYLOAD_TYPE);
        if (!dest)
        {
            log_error("Failed to map RTP socket");
            return -1;
        }

        header->timestamp = htonl(dest->timestamp);
        header->payload_type = dest->pl_type;

        dest->timestamp += RTP_TIMESTAMP_INCREMENT;

        return udp_send(socket->udp_sock, buffer, total_len);
    }
    else
    {
        rtp_dest_t *dest = rtp_dest_find(socket, ssrc);
        if (!dest)
        {
            log_error("Failed to find address for SSRC#%u", ssrc);
            return -1;
        }

        header->timestamp = htonl(dest->timestamp);
        header->payload_type = dest->pl_type;

        dest->timestamp += RTP_TIMESTAMP_INCREMENT;

        return udp_sendto(socket->udp_sock, buffer, total_len, &dest->addr, dest->addr_len);
    }
}

int rtp_close_stream(rtp_socket_t *socket, ssrc_t ssrc)
{
    return rtp_dest_del(socket, ssrc);
}

ssrc_t rtp_random_ssrc(rtp_socket_t *socket)
{
    ssrc_t ssrc;
    randombytes_buf(&ssrc, sizeof(ssrc));

    while (rtp_dest_find(socket, ssrc))
    {
        randombytes_buf(&ssrc, sizeof(ssrc));
    }

    return ssrc;
}

rtp_dest_t *rtp_dest_find(rtp_socket_t *socket, ssrc_t ssrc)
{
    rtp_dest_t *res = NULL;
    HASH_FIND(hh, socket->rtp_dest_map, &ssrc, sizeof(ssrc_t), res);

    return res;
}

rtp_dest_t *rtp_dest_set(rtp_socket_t *socket, ssrc_t ssrc, struct sockaddr_storage *address,
                         socklen_t address_len, uint8_t payload_type)
{
    rtp_dest_t *existing = rtp_dest_find(socket, ssrc);
    if (existing)
    {
        if (memcmp(&existing->addr, address, address_len) == 0)
        {
            return existing;
        }
        else
        {
            HASH_DEL(socket->rtp_dest_map, existing);
            free(existing);
        }
    }

    rtp_dest_t *dest = malloc(sizeof(*dest));
    if (!dest)
    {
        elog_error("malloc(rtp_dest_t) failed");
        return NULL;
    }
    dest->ssrc = ssrc;
    dest->addr_len = address_len;
    memcpy(&dest->addr, address, address_len);

    randombytes_buf(&dest->timestamp, sizeof(dest->timestamp));
    dest->pl_type = payload_type;

    HASH_ADD(hh, socket->rtp_dest_map, ssrc, sizeof(ssrc_t), dest);

    return dest;
}

int rtp_dest_del(rtp_socket_t *socket, ssrc_t ssrc)
{
    rtp_dest_t *deletee = rtp_dest_find(socket, ssrc);
    if (!deletee)
        return -1;

    HASH_DEL(socket->rtp_dest_map, deletee);
    free(deletee);

    return 0;
}

void rtp_dest_free(rtp_socket_t *socket)
{
    if (!socket->rtp_dest_map)
        return;

    rtp_dest_t *current, *tmp;
    HASH_ITER(hh, socket->rtp_dest_map, current, tmp)
    {
        HASH_DEL(socket->rtp_dest_map, current);
        free(current);
    }
}

void udp_recv_callback(udp_socket_t *socket, char *data, ssize_t data_len,
                       struct sockaddr_storage *address, socklen_t addrlen)
{
    if (data_len <= sizeof(rtphdr_t) + CHACHA_MAC_LEN + CHACHA_NONCE_LEN)
    {
        log_debug("Received packet with invalid size");
        return;
    }

    rtphdr_t *header = (rtphdr_t *)data;
    if (header->version != 2)
    {
        log_debug("Received packet with invalid RTP version");
        return;
    }

    rtp_socket_t *rtp_sock = socket->user_data;
    ssrc_t ssrc = ntohl(header->ssrc);

    char *cipher = (data + sizeof(rtphdr_t));
    size_t payload_len = data_len - (sizeof(rtphdr_t) + CHACHA_MAC_LEN + CHACHA_NONCE_LEN);

    char dec_payload[RTP_MAX_PAYLOAD_SIZE];
    if (chacha_decrypt(&rtp_sock->cipher, cipher, payload_len,
                       &data[data_len - CHACHA_MAC_LEN],
                       &data[data_len - (CHACHA_NONCE_LEN + CHACHA_MAC_LEN)],
                       dec_payload) != 0)
    {
        log_error("Failed to decrypt data");
        return;
    }

    // Map SSRC to socket address if listening socket
    if (!rtp_sock->connected)
    {
        if (!rtp_dest_set(rtp_sock, ssrc, address, addrlen, header->payload_type))
            log_error("Failed to map RTP socket");
    }

    if (rtp_sock->recv_cb)
        ((rtp_recv_callback_t)rtp_sock->recv_cb)(rtp_sock, dec_payload, payload_len, ssrc);
}

void udp_send_callback(udp_socket_t *socket, ssize_t sent)
{
    rtp_socket_t *rtp_sock = socket->user_data;

    if (rtp_sock->send_cb)
        ((rtp_send_callback_t)rtp_sock->send_cb)(rtp_sock, sent - sizeof(rtphdr_t));
}