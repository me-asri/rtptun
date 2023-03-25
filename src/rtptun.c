#include "rtptun.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>

#include <unistd.h>
#include <getopt.h>

#include <ev.h>

#include "log.h"
#include "crypto/chacha.h"
#include "server.h"
#include "client.h"

#ifndef BUILD_VERSION
#define BUILD_VERSION "undefined version"
#endif
#ifdef DEBUG
#define BUILD_TYPE "debug"
#else
#define BUILD_TYPE "release"
#endif

typedef enum action
{
    ACT_GEN_KEY,
    ACT_CLIENT,
    ACT_SERVER,
    ACT_INVALID,
} action_t;

static void print_usage(const char *prog, FILE *out);
static void print_version();
static void argerror(const char *prog, const char *format, ...);
static action_t parse_action(const char *action);

static void signal_callback(EV_P_ ev_signal *w, int revents);
static void watch_signals(EV_P);

ev_signal sigint_watcher, sigterm_watcher;

int main(int argc, char *argv[])
{
    const char *key = NULL;
    const char *listen_addr = NULL;
    const char *listen_port = NULL;
    const char *dest_addr = NULL;
    const char *dest_port = NULL;
    log_level_t log_level = DEFAULT_LOG_LEVEL;

    char *action_arg = argv[1];
    if (action_arg && action_arg[0] != '-')
    {
        argv[1] = argv[0];
        argv++;
        argc--;
    }
    else
    {
        action_arg = NULL;
    }

    int opt;
    while ((opt = getopt(argc, argv, "i:l:d:p:k:p:hVv")) != -1)
    {
        switch (opt)
        {
        case 'i':
            listen_addr = optarg;
            break;
        case 'l':
            listen_port = optarg;
            break;
        case 'd':
            dest_addr = optarg;
            break;
        case 'p':
            dest_port = optarg;
            break;
        case 'k':
            key = optarg;
            break;
        case 'v':
            log_level = LOG_DEBUG;
            break;
        case 'h':
            print_usage(argv[0], stdout);
            return 0;
        case 'V':
            print_version();
            return 0;
        case '?':
            print_usage(argv[0], stderr);
            return 1;
        default:
            argerror(argv[0], "unhandled option: %c", opt);
        }
    }

    log_init(log_level);

    if (!action_arg)
        argerror(argv[0], "missing action argument");
    if (argc - optind != 0)
        argerror(argv[0], "invalid number of non-option arguments");

    switch (parse_action(action_arg))
    {
    case ACT_GEN_KEY:
    {
        char *key = chacha_gen_key();
        puts(key);
        free(key);

        return 0;
    }
    case ACT_CLIENT:
    {
        if (!key)
            argerror(argv[0], "encryption key not specified");
        if (!listen_addr)
            listen_addr = RTPTUN_DEFAULT_CLIENT_LISTEN;
        if (!listen_port)
            argerror(argv[0], "listen port not specified");
        if (!dest_addr)
            argerror(argv[0], "destination address not specified");
        if (!dest_port)
            dest_port = RTPTUN_DEFAULT_SERVER_PORT;

        struct ev_loop *loop = EV_DEFAULT;
        watch_signals(loop);

        rtptun_client_t *client = rtptun_client_new(loop, listen_addr, listen_port,
                                                    dest_addr, dest_port, key);
        if (!client)
            return 1;

        log_i("Tunneling [%s]:%s to [%s]:%s", listen_addr, listen_port, dest_addr, dest_port);

        ev_run(loop, 0);

        rtptun_client_free(client);
        return 0;
    }
    case ACT_SERVER:
    {
        if (!key)
            argerror(argv[0], "encryption key not specified");
        if (!listen_addr)
            listen_addr = RTPTUN_DEFAULT_SERVER_LISTEN;
        if (!listen_port)
            listen_port = RTPTUN_DEFAULT_SERVER_PORT;
        if (!dest_addr)
            dest_addr = RTPTUN_DEFAULT_DEST_ADDR;
        if (!dest_port)
            argerror(argv[0], "destination port not specified");

        struct ev_loop *loop = EV_DEFAULT;
        watch_signals(loop);

        rtptun_server_t *server = rtptun_server_new(loop, listen_addr, listen_port,
                                                    dest_addr, dest_port, key);
        if (!server)
            return 1;

        log_i("Tunneling [%s]:%s to [%s]:%s", listen_addr, listen_port, dest_addr, dest_port);

        ev_run(loop, 0);

        rtptun_server_free(server);
        return 0;
    }
    case ACT_INVALID:
    {
        argerror(argv[0], "invalid action '%s'", action_arg);
    }
    default:
    {
        argerror(argv[0], "unhandled action '%s'", action_arg);
    }
    }

    return 0;
}

void print_usage(const char *prog, FILE *out)
{
    static const char MESSAGE[] = "Usage: %1$s <action> <options>\n"
                                  "Example:\n"
                                  " - Generate key: %1$s genkey\n"
                                  " - Run server:   %1$s server -k <KEY> -l 5004 -p 1194\n"
                                  " - Run client:   %1$s client -k <KEY> -l 1194 -d 192.0.2.1 -p 5004\n"
                                  "\n"
                                  "Actions:\n"
                                  "  client  : run as client\n"
                                  "  server  : run as server\n"
                                  "  genkey  : generate encryption key\n"
                                  "\n"
                                  "Server options:\n"
                                  "  -i : listen address (default: " RTPTUN_DEFAULT_SERVER_LISTEN ")\n"
                                  "  -l : listen port (default: " RTPTUN_DEFAULT_SERVER_PORT ")\n"
                                  "  -d : destination address (default: " RTPTUN_DEFAULT_DEST_ADDR ")\n"
                                  "  -p : destination port\n"
                                  "\n"
                                  "Client options:\n"
                                  "  -i : listen address (default: " RTPTUN_DEFAULT_CLIENT_LISTEN ")\n"
                                  "  -l : listen port\n"
                                  "  -d : server address\n"
                                  "  -p : server port (default: " RTPTUN_DEFAULT_SERVER_PORT ")\n"
                                  "\n"
                                  "Common options:\n"
                                  "  -k : encryption key\n"
                                  "  -h : display help message\n"
                                  "  -v : verbose\n"
                                  "  -V : display version information\n";
    fprintf(out, MESSAGE, prog);
}

void print_version()
{
    static const char MESSAGE[] = "rtptun " BUILD_VERSION " (" BUILD_TYPE " build)\n"
                                  "Copyright (C) 2023 Mehrzad Asri\n"
                                  "Licensed under MIT\n\n"
                                  "This is free software; you are free to change and redistribute it.\n"
                                  "There is NO WARRANTY, to the extent permitted by law.\n\n"
                                  "Third-party licenses:\n"
#ifdef __CYGWIN__
                                  "- Cygwin - LGPLv3 - <https://www.cygwin.com/>\n"
#endif
                                  "- libsodium - ISC - <https://doc.libsodium.org/>\n"
                                  "- libev - BSD-2-Clause - <http://software.schmorp.de/pkg/libev.html>\n"
                                  "- uthash - BSD revised - <https://troydhanson.github.io/uthash/>";
    puts(MESSAGE);
}

void argerror(const char *prog, const char *format, ...)
{
    fprintf(stderr, "%s: ", prog);

    va_list args;
    va_start(args, format);

    vfprintf(stderr, format, args);

    va_end(args);

    fputc('\n', stderr);
    fprintf(stderr, "Try '%s -h' for more information.\n", prog);

    exit(1);
}

action_t parse_action(const char *action)
{
    if (strcmp(action, "client") == 0)
        return ACT_CLIENT;
    else if (strcmp(action, "server") == 0)
        return ACT_SERVER;
    else if (strcmp(action, "genkey") == 0)
        return ACT_GEN_KEY;

    return ACT_INVALID;
}

void signal_callback(EV_P_ ev_signal *w, int revents)
{
    log_i("Bye bye!");
    ev_break(EV_A_ EVBREAK_ALL);
}

void watch_signals(EV_P)
{
    ev_signal_init(&sigint_watcher, signal_callback, SIGINT);
    ev_signal_init(&sigterm_watcher, signal_callback, SIGTERM);

    ev_signal_start(loop, &sigint_watcher);
    ev_signal_start(loop, &sigterm_watcher);
}