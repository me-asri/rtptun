#define _POSIX_C_SOURCE 200809L

#include "server.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <ev.h>

#include "ext/uthash.h"

#include "proto/rtp.h"
#include "proto/udp.h"

#include "rtptun.h"
#include "log.h"

static void rtp_recv_cb(rtp_socket_t *socket, unsigned char *data, ssize_t data_len, ssrc_t ssrc);
static void udp_recv_cb(udp_socket_t *socket, unsigned char *data, ssize_t data_len,
                        struct sockaddr_storage *address, socklen_t addrlen);
static void timeout_cb(EV_P_ ev_timer *timer, int revents);

static rtptun_rtp_info_t *info_map_set(rtptun_rtp_info_t **hash, ssrc_t ssrc, rtp_socket_t *rtp, udp_socket_t *sock);
static rtptun_rtp_info_t *info_map_find(rtptun_rtp_info_t **hash, ssrc_t ssrc);
static void info_map_free(rtptun_rtp_info_t **hash);

rtptun_server_t *rtptun_server_new(struct ev_loop *loop, const char *listen_addr, const char *listen_port,
                                   const char *dest_addr, const char *dest_port, const char *key)
{
    rtptun_server_t *server = calloc(1, sizeof(*server));
    if (!server)
    {
        elog_e("calloc(rtptun_server_t) failed");
        goto error;
    }

    server->loop = loop;
    server->dest_addr = strdup(dest_addr);
    server->dest_port = strdup(dest_port);
    server->info_map = NULL;

    server->local_rtp = rtp_listen(loop, listen_addr, listen_port, key, rtp_recv_cb, NULL, server);
    if (!server->local_rtp)
    {
        log_e("Failed to create RTP socket");
        goto error;
    }

    ev_timer_init(&server->to_timer, timeout_cb, 0, RTPTUN_TIMEOUT);
    server->to_timer.data = server;
    ev_timer_start(loop, &server->to_timer);

    return server;
error:
    if (server)
    {
        if (server->local_rtp)
            rtp_destroy(server->local_rtp);

        free(server);
    }

    return NULL;
}

void rtptun_server_free(rtptun_server_t *server)
{
    rtp_destroy(server->local_rtp);

    free(server->dest_addr);
    free(server->dest_port);

    info_map_free(&server->info_map);

    ev_timer_stop(server->loop, &server->to_timer);

    free(server);
}

rtptun_rtp_info_t *info_map_set(rtptun_rtp_info_t **hash, ssrc_t ssrc, rtp_socket_t *rtp, udp_socket_t *sock)
{
    rtptun_rtp_info_t *info = malloc(sizeof(*info));
    if (!info)
    {
        elog_e("malloc(rtptun_rtp_info_t) failed");
        return NULL;
    }

    info->ssrc = ssrc;
    info->remote_udp = sock;
    info->local_rtp = rtp;
    info->active = true;

    HASH_ADD(hh, *hash, ssrc, sizeof(info->ssrc), info);
    return info;
}

rtptun_rtp_info_t *info_map_find(rtptun_rtp_info_t **hash, ssrc_t ssrc)
{
    rtptun_rtp_info_t *result;

    HASH_FIND(hh, *hash, &ssrc, sizeof(result->ssrc), result);
    return result;
}

void info_map_free(rtptun_rtp_info_t **hash)
{
    rtptun_rtp_info_t *current, *tmp;
    HASH_ITER(hh, *hash, current, tmp)
    {
        udp_destroy(current->remote_udp);

        HASH_DEL(*hash, current);
        free(current);
    }
}

void rtp_recv_cb(rtp_socket_t *socket, unsigned char *data, ssize_t data_len, ssrc_t ssrc)
{
    rtptun_server_t *server = socket->user_data;

    rtptun_rtp_info_t *info = info_map_find(&server->info_map, ssrc);
    if (!info)
    {
        udp_socket_t *udp_out = udp_connect(server->loop, server->dest_addr, server->dest_port,
                                            udp_recv_cb, NULL, NULL);
        if (!udp_out)
        {
            log_e("Failed to connect to [%s]:%s", server->dest_addr, server->dest_port);
            return;
        }

        info = info_map_set(&server->info_map, ssrc, server->local_rtp, udp_out);
        if (!info)
        {
            log_e("Failed to map UDP socket");
            return;
        }

        udp_out->user_data = info;
    }

    info->active = true;

    if (udp_send(info->remote_udp, data, data_len) != 0)
        log_e("Failed to send UDP packet");
}

void udp_recv_cb(udp_socket_t *socket, unsigned char *data, ssize_t data_len,
                 struct sockaddr_storage *address, socklen_t addrlen)
{
    rtptun_rtp_info_t *info = socket->user_data;

    info->active = true;

    if (rtp_send(info->local_rtp, data, data_len, info->ssrc) != 0)
        log_e("Failed to send RTP packet");
}

void timeout_cb(EV_P_ ev_timer *timer, int revents)
{
    rtptun_server_t *server = timer->data;

    rtptun_rtp_info_t *current, *tmp;
    HASH_ITER(hh, server->info_map, current, tmp)
    {
        if (current->active)
        {
            current->active = false;
        }
        else
        {
            log_d("Client with SSRC #%d timed out", current->ssrc);
            udp_destroy(current->remote_udp);

            HASH_DEL(server->info_map, current);
            free(current);
        }
    }
}