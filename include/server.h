#ifndef RTPTUN_SERVER_H
#define RTPTUN_SERVER_H

#include <stdint.h>
#include <stdbool.h>

#include <ev.h>

#include "ext/uthash.h"

#include "proto/rtp.h"
#include "proto/udp.h"

typedef struct rtptun_rtp_info
{
    ssrc_t ssrc;
    rtp_socket_t *local_rtp;
    udp_socket_t *remote_udp;

    bool active;

    UT_hash_handle hh;
} rtptun_rtp_info_t;

typedef struct rtptun_server
{
    struct ev_loop *loop;

    char *dest_addr;
    char *dest_port;

    ev_timer to_timer;

    rtp_socket_t *local_rtp;
    rtptun_rtp_info_t *info_map;
} rtptun_server_t;

rtptun_server_t *rtptun_server_new(struct ev_loop *loop, const char *listen_addr, const char *listen_port,
                                   const char *dest_addr, const char *dest_port, const char *key);
void rtptun_server_free(rtptun_server_t *server);

#endif