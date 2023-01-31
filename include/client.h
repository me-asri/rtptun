#ifndef RTPTUN_CLIENT_H
#define RTPTUN_CLIENT_H

#include <stdbool.h>

#ifdef __MINGW32__
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#endif

#include "proto/udp.h"
#include "proto/rtp.h"

#include <ev.h>

#include "ext/uthash.h"

typedef struct rtptun_udp_info
{
    struct sockaddr_storage saddr;

    ssrc_t ssrc;

    bool active;

    UT_hash_handle hh;
} rtptun_udp_info_t;

typedef struct rtptun_client
{
    udp_socket_t *udp_local;
    rtp_socket_t *rtp_remote;

    struct ev_loop *loop;
    ev_timer to_timer;

    socklen_t udp_addr_len;

    rtptun_udp_info_t *info_map;
    rtptun_udp_info_t *info_map_reverse;
} rtptun_client_t;

rtptun_client_t *rtptun_client_new(struct ev_loop *loop, const char *local_addr, const char *local_port,
                                   const char *remote_addr, const char *remote_port, const char *key);
void rtptun_client_free(rtptun_client_t *client);

#endif