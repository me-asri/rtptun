#define _POSIX_C_SOURCE 200112L

#include "proto/udp.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>

#include "log.h"

static int socket_set_nonblock(int fd);
static int socket_parse_addr(const char *address, const char *port, struct sockaddr_storage *saddress, socklen_t *saddress_len);

static void ev_callback(EV_P_ ev_io *io, int events);

udp_socket_t *udp_connect(struct ev_loop *loop, const char *address, const char *port,
                          udp_recv_callback_t recv_callback, udp_send_callback_t send_callback, void *user_data)
{
    udp_socket_t *sock = malloc(sizeof(*sock));
    if (!sock)
    {
        elog_e("malloc(udp_socket_t) failed");
        goto error;
    }

    sock->loop = loop;
    sock->recv_callback = (void *)recv_callback;
    sock->send_callback = (void *)send_callback;
    sock->user_data = user_data;

    struct addrinfo hints = {
        .ai_flags = AI_NUMERICSERV,
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_DGRAM,
    };
    struct in6_addr addr = {};
    if (inet_pton(AF_INET, address, &addr) == 1)
    {
        hints.ai_family = AF_INET;
        hints.ai_flags |= AI_NUMERICHOST;
    }
    else if (inet_pton(AF_INET6, address, &addr) == 1)
    {
        hints.ai_family = AF_INET6;
        hints.ai_flags |= AI_NUMERICHOST;
    }
    struct addrinfo *res = NULL;
    int result = getaddrinfo(address, port, &hints, &res);
    if (result != 0)
    {
        log_e("Failed to resolve %s:%s (%s)", address, port, gai_strerror(result));
        if (result == EAI_SYSTEM)
            elog_e("getaddrinfo() failed");
        goto error;
    }

    sock->fd = socket(res[0].ai_family, res[0].ai_socktype, res[0].ai_protocol);
    if (sock->fd < 0)
    {
        elog_e("socket() failed");
        goto error;
    }

    sock->local_address_len = 0;

    sock->remote_address_len = res[0].ai_addrlen;
    memcpy(&sock->remote_address, res[0].ai_addr, res[0].ai_addrlen);

    if (socket_set_nonblock(sock->fd) != 0)
    {
        elog_e("Failed to make socket non-blocking");
        goto error;
    }

    ev_io_init(&sock->ev, ev_callback, sock->fd, EV_READ);
    sock->ev.data = sock;
    ev_io_start(loop, &sock->ev);

    freeaddrinfo(res);
    return sock;
error:
    if (sock)
    {
        if (res)
            freeaddrinfo(res);

        free(sock);
    }

    return NULL;
}

udp_socket_t *udp_listen(struct ev_loop *loop, const char *address, const char *port,
                         udp_recv_callback_t recv_callback, udp_send_callback_t send_callback, void *user_data)
{
    udp_socket_t *sock = malloc(sizeof(*sock));
    if (!sock)
    {
        elog_e("malloc(udp_socket_t) failed");
        goto error;
    }

    sock->remote_address_len = 0;

    sock->local_address_len = sizeof(sock->local_address);
    if (socket_parse_addr(address, port, &sock->local_address, &sock->local_address_len) != 0)
    {
        log_e("Failed to parse address");
        goto error;
    }

    sock->loop = loop;
    sock->recv_callback = recv_callback;
    sock->send_callback = send_callback;
    sock->user_data = user_data;

    sock->fd = socket(sock->local_address.ss_family, SOCK_DGRAM, 0);
    if (sock->fd < 0)
    {
        elog_e("socket() failed");
        goto error;
    }

    if (bind(sock->fd, (struct sockaddr *)&sock->local_address, sock->local_address_len) != 0)
    {
        elog_e("bind() failed");
        goto error;
    }

    ev_io_init(&sock->ev, ev_callback, sock->fd, EV_READ);
    sock->ev.data = sock;
    ev_io_start(loop, &sock->ev);

    return sock;
error:
    if (sock)
        free(sock);

    return NULL;
}

void udp_destroy(udp_socket_t *socket)
{
    ev_io_stop(socket->loop, &socket->ev);

    close(socket->fd);

    free(socket);
}

int udp_send(udp_socket_t *socket, const unsigned char *data, size_t data_len)
{
    if (socket->remote_address_len == 0)
    {
        log_e("Socket not connected");
        return -1;
    }

    return udp_sendto(socket, data, data_len, &socket->remote_address, socket->remote_address_len);
}

int udp_sendto(udp_socket_t *socket, const unsigned char *data, size_t data_len,
               struct sockaddr_storage *address, socklen_t addr_len)
{
    if (data_len > UDP_BUFFER_SIZE)
    {
        log_e("Maximum UDP buffer size exceeded");
        return -1;
    }

    ssize_t sent = sendto(socket->fd, data, data_len, 0, (struct sockaddr *)address, addr_len);
    if (sent < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            if (socket->out_buffer.data_len > 0)
                log_w("Send buffer overrun");

            memcpy(&socket->out_buffer.saddr, address, addr_len);
            socket->out_buffer.saddr_len = addr_len;
            memcpy(socket->out_buffer.data, data, data_len);
            socket->out_buffer.data_len = data_len;

            ev_io_set(&socket->ev, socket->fd, EV_READ | EV_WRITE);

            return 0;
        }
        else
        {
            elog_w("sendto() failed");
            return -1;
        }
    }

    return 0;
}

int socket_set_nonblock(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        return -1;

    flags &= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) != 0)
        return -1;

    return 0;
}

int socket_parse_addr(const char *address, const char *port, struct sockaddr_storage *saddress, socklen_t *saddress_len)
{
    char *endptr;
    long port_num = strtol(port, &endptr, 10);
    if (*endptr != '\0')
        return -1;
    if (port_num > UINT16_MAX)
        return -1;

    struct sockaddr_in *sin = (struct sockaddr_in *)saddress;
    struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)saddress;
    if (inet_pton(AF_INET, address, &sin->sin_addr) == 1)
    {
        sin->sin_family = AF_INET;
        sin->sin_port = htons(port_num);

        *saddress_len = sizeof(*sin);
        return 0;
    }
    if (inet_pton(AF_INET6, address, &sin6->sin6_addr) == 1)
    {
        sin6->sin6_family = AF_INET6;
        sin6->sin6_port = htons(port_num);

        *saddress_len = sizeof(*sin6);
        return 0;
    }

    return -1;
}

void ev_callback(EV_P_ ev_io *io, int events)
{
    udp_socket_t *sock = (udp_socket_t *)io->data;

    if (events & EV_WRITE)
    {
        ssize_t sent = sendto(sock->fd, sock->out_buffer.data, sock->out_buffer.data_len, 0,
                              (struct sockaddr *)&sock->out_buffer.saddr, sock->out_buffer.saddr_len);

        sock->out_buffer.data_len = 0;
        ev_io_set(&sock->ev, sock->fd, EV_READ);

        if (sent < 0)
        {
            elog_w("sendto() failed");
            return;
        }

        if (sock->send_callback)
            sock->send_callback(sock, sent);
    }
    else if (events & EV_READ)
    {
        unsigned char buffer[UDP_BUFFER_SIZE];

        struct sockaddr_storage saddr;
        socklen_t addr_len = sizeof(saddr);
        ssize_t nread = recvfrom(sock->fd, buffer, sizeof(buffer), 0, (struct sockaddr *)&saddr, &addr_len);
        if (nread < 0)
        {
            elog_w("recvfrom() failed");
            return;
        }
        if (sock->remote_address_len > 0 && memcmp(&sock->remote_address, &saddr, sock->remote_address_len) != 0)
        {
            log_d("Dropping packet received from non-connected party");
            return;
        }

        if (sock->recv_callback)
            sock->recv_callback(sock, buffer, nread, &saddr, addr_len);
    }
}