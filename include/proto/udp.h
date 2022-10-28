#ifndef RTPTUN_PROTO_UDP_H
#define RTPTUN_PROTO_UDP_H

#define UDP_BUFFER_SIZE 4096

#include <stddef.h>

#include <sys/types.h>
#include <netinet/in.h>

#include <ev.h>

typedef struct udp_buffer
{
    char data[UDP_BUFFER_SIZE];
    size_t data_len;

    struct sockaddr_storage saddr;
    socklen_t saddr_len;
} udp_buffer_t;

typedef struct udp_socket
{
    int fd;
    int connected;

    struct sockaddr_storage address;
    socklen_t address_length;

    struct ev_loop *loop;
    ev_io ev;

    udp_buffer_t out_buffer;

    void *send_callback; // udp_send_callback_t
    void *recv_callback; // udp_recv_callback_t
    void *user_data;
} udp_socket_t;

typedef void (*udp_send_callback_t)(udp_socket_t *socket, ssize_t sent);
typedef void (*udp_recv_callback_t)(udp_socket_t *socket, char *data, ssize_t data_len,
                                    struct sockaddr_storage *address, socklen_t addr_len);

udp_socket_t *udp_connect(struct ev_loop *loop, const char *address, const char *port,
                          udp_recv_callback_t recv_callback, udp_send_callback_t send_callback, void *user_data);
udp_socket_t *udp_listen(struct ev_loop *loop, const char *address, const char *port,
                         udp_recv_callback_t recv_callback, udp_send_callback_t send_callback, void *user_data);
void udp_free(udp_socket_t *socket);

int udp_send(udp_socket_t *socket, const char *data, size_t data_len);
int udp_sendto(udp_socket_t *socket, const char *data, size_t data_len,
               struct sockaddr_storage *address, socklen_t addr_len);

#endif