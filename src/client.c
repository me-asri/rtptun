#include "client.h"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <ev.h>

#include "rtptun.h"
#include "log.h"
#include "proto/udp.h"
#include "proto/rtp.h"

static void udp_recv_cb(udp_socket_t *socket, unsigned char *data, ssize_t data_len,
                        struct sockaddr_storage *address, socklen_t addr_len);
static void rtp_recv_cb(rtp_socket_t *socket, unsigned char *data, ssize_t data_len, ssrc_t ssrc);
static void timeout_cb(EV_P_ ev_timer *timer, int revents);

static rtptun_udp_info_t *info_map_set(rtptun_client_t *client, struct sockaddr_storage *saddr, ssrc_t ssrc);
static rtptun_udp_info_t *info_map_find(rtptun_client_t *client, struct sockaddr_storage *saddr);
static rtptun_udp_info_t *info_map_find_reverse(rtptun_client_t *client, ssrc_t ssrc);
static void info_map_free(rtptun_client_t *client);

rtptun_client_t *rtptun_client_new(struct ev_loop *loop, const char *local_addr, const char *local_port,
                                   const char *remote_addr, const char *remote_port, const char *key)
{
    rtptun_client_t *client = calloc(1, sizeof(*client));
    if (!client)
    {
        elog_error("calloc(rtptun_client_t) failed");
        goto error;
    }

    client->loop = loop;
    client->info_map = NULL;
    client->info_map_reverse = NULL;

    client->udp_local = udp_listen(loop, local_addr, local_port, udp_recv_cb, NULL, client);
    if (!client->udp_local)
    {
        log_error("Failed to create local UDP socket");
        goto error;
    }

    client->udp_addr_len = client->udp_local->local_address_len;

    client->rtp_remote = rtp_connect(loop, remote_addr, remote_port, key, rtp_recv_cb, NULL, client);
    if (!client->rtp_remote)
    {
        log_error("Failed to create remote RTP socket");
        goto error;
    }

    ev_timer_init(&client->to_timer, timeout_cb, 0, RTPTUN_TIMEOUT);
    client->to_timer.data = client;
    ev_timer_start(loop, &client->to_timer);

    return client;
error:
    if (client)
    {
        if (client->udp_local)
        {
            udp_free(client->udp_local);

            if (client->rtp_remote)
                rtp_free(client->rtp_remote);
        }

        free(client);
    }

    return NULL;
}

void rtptun_client_free(rtptun_client_t *client)
{
    ev_timer_stop(client->loop, &client->to_timer);

    info_map_free(client);

    udp_free(client->udp_local);
    rtp_free(client->rtp_remote);

    free(client);
}

static rtptun_udp_info_t *info_map_set(rtptun_client_t *client, struct sockaddr_storage *saddr, ssrc_t ssrc)
{
    rtptun_udp_info_t *info = NULL, *rev_info = NULL;

    info = malloc(sizeof(*info));
    if (!info)
    {
        elog_error("malloc(rtptun_udp_info_t) failed");
        goto error;
    }
    memcpy(&info->saddr, saddr, client->udp_addr_len);
    info->ssrc = ssrc;
    info->active = true;

    rev_info = malloc(sizeof(*rev_info));
    if (!rev_info)
    {
        elog_error("malloc(rtptun_udp_info_t) failed");
        goto error;
    }
    memcpy(rev_info, info, sizeof(*info));

    HASH_ADD(hh, client->info_map, saddr, client->udp_addr_len, info);
    HASH_ADD(hh, client->info_map_reverse, ssrc, sizeof(ssrc), rev_info);

    return info;
error:
    if (info)
        free(info);
    if (rev_info)
        free(rev_info);

    return NULL;
}

rtptun_udp_info_t *info_map_find(rtptun_client_t *client, struct sockaddr_storage *saddr)
{
    rtptun_udp_info_t *info;

    HASH_FIND(hh, client->info_map, saddr, client->udp_addr_len, info);
    return info;
}

static rtptun_udp_info_t *info_map_find_reverse(rtptun_client_t *client, ssrc_t ssrc)
{
    rtptun_udp_info_t *info;

    HASH_FIND(hh, client->info_map_reverse, &ssrc, sizeof(info->ssrc), info);
    return info;
}

void info_map_free(rtptun_client_t *client)
{
    rtptun_udp_info_t *current, *tmp;

    HASH_ITER(hh, client->info_map, current, tmp)
    {
        HASH_DEL(client->info_map, current);
        free(current);
    }
    HASH_ITER(hh, client->info_map_reverse, current, tmp)
    {
        HASH_DEL(client->info_map_reverse, current);
        free(current);
    }
}

void udp_recv_cb(udp_socket_t *socket, unsigned char *data, ssize_t data_len,
                 struct sockaddr_storage *address, socklen_t addr_len)
{
    rtptun_client_t *client = socket->user_data;

    rtptun_udp_info_t *info = info_map_find(client, address);
    if (!info)
    {
        log_debug("Received packet from new sender");

        info = info_map_set(client, address, rtp_random_ssrc(client->rtp_remote));
        if (!info)
        {
            log_error("Failed to map local socket");
            return;
        }
    }

    info->active = true;

    if (rtp_send(client->rtp_remote, data, data_len, info->ssrc) != 0)
        log_error("Failed to send RTP packet");
}

void rtp_recv_cb(rtp_socket_t *socket, unsigned char *data, ssize_t data_len, ssrc_t ssrc)
{
    rtptun_client_t *client = socket->user_data;

    rtptun_udp_info_t *info = info_map_find_reverse(client, ssrc);
    if (!info)
    {
        log_debug("Received packet from unrecognized SSRC #%d", ssrc);
        return;
    }

    info->active = true;

    if (udp_sendto(client->udp_local, data, data_len, &info->saddr, client->udp_addr_len) != 0)
        log_error("Failed to send UDP packet");
}

void timeout_cb(EV_P_ ev_timer *timer, int revents)
{
    rtptun_client_t *client = timer->data;

    rtptun_udp_info_t *current, *tmp, *reverse;
    HASH_ITER(hh, client->info_map, current, tmp)
    {
        reverse = info_map_find_reverse(client, current->ssrc);

        if (current->active || reverse->active)
        {
            current->active = false;
            reverse->active = false;
        }
        else
        {
            log_debug("Connection associated with SSRC #%d timed out", current->ssrc);

            HASH_DEL(client->info_map_reverse, reverse);
            HASH_DEL(client->info_map, current);

            free(reverse);
            free(current);
        }
    }
}