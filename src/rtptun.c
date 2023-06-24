#include "rtptun.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>

#include <unistd.h>
#include <getopt.h>

#include <ev.h>

#include "log.h"
#include "config.h"
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

static const char *prog_name = NULL;

static void print_usage(FILE *out);
static void print_version();
static void argerror(const char *format, ...);
static action_t parse_action(const char *action);

static void signal_callback(EV_P_ ev_signal *w, int revents);
static void watch_signals(EV_P);

static int start_server(const char *listen_addr, const char *listen_port,
                        const char *dest_addr, const char *dest_port, const char *key);
static int start_client(const char *listen_addr, const char *listen_port,
                        const char *dest_addr, const char *dest_port, const char *key);
static int gen_key();

ev_signal sigint_watcher, sigterm_watcher;

int main(int argc, char *argv[])
{
    prog_name = argv[0];

    const char *config_file = NULL;

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
    while ((opt = getopt(argc, argv, "i:l:d:p:k:p:f:hVv")) != -1)
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
        case 'f':
            config_file = optarg;
            break;
        case 'v':
            log_level = LOG_DEBUG;
            break;
        case 'h':
            print_usage(stdout);
            return 0;
        case 'V':
            print_version();
            return 0;
        case '?':
            print_usage(stderr);
            return 1;
        default:
            argerror("unhandled option: %c", opt);
        }
    }

    log_init(log_level);

    int ret = 0;
    if (config_file)
    {
        log_i("Loading configuration file %s", config_file);

        config_t cfg;
        if (config_open(&cfg, config_file) != 0)
            log_f("Failed top open config file");

        if (config_has_section(&cfg, "client") && config_has_section(&cfg, "server"))
            log_f("Config file may only contain either a 'client' or a 'server' section");

        if (config_has_section(&cfg, "client"))
        {
            config_get_str(&cfg, "client", "local-addr", &listen_addr);
            config_get_str(&cfg, "client", "local-port", &listen_port);
            config_get_str(&cfg, "client", "server-addr", &dest_addr);
            config_get_str(&cfg, "client", "server-port", &dest_port);
            config_get_str(&cfg, "client", "key", &key);

            ret = start_client(listen_addr, listen_port, dest_addr, dest_port, key);
        }
        else if (config_has_section(&cfg, "server"))
        {
            config_get_str(&cfg, "server", "listen-addr", &listen_addr);
            config_get_str(&cfg, "server", "listen-port", &listen_port);
            config_get_str(&cfg, "server", "dest-addr", &dest_addr);
            config_get_str(&cfg, "server", "dest-port", &dest_port);
            config_get_str(&cfg, "server", "key", &key);

            ret = start_server(listen_addr, listen_port, dest_addr, dest_port, key);
        }
        else
        {
            log_f("Invalid config file");
        }

        config_free(&cfg);
    }
    else
    {
        if (!action_arg)
            argerror("missing action argument");
        if (argc - optind != 0)
            argerror("invalid number of non-option arguments");

        switch (parse_action(action_arg))
        {
        case ACT_GEN_KEY:
            ret = gen_key();

            break;
        case ACT_CLIENT:
            ret = start_client(listen_addr, listen_port, dest_addr, dest_port, key);

            break;
        case ACT_SERVER:
            ret = start_server(listen_addr, listen_port, dest_addr, dest_port, key);

            break;
        default:
            argerror("invalid action '%s'", action_arg);
        }
    }

    return ret;
}

int start_client(const char *listen_addr, const char *listen_port,
                 const char *dest_addr, const char *dest_port, const char *key)
{
    if (!key)
        argerror("encryption key not specified");
    if (!listen_addr)
        listen_addr = RTPTUN_DEFAULT_CLIENT_LISTEN;
    if (!listen_port)
        argerror("local port not specified");
    if (!dest_addr)
        argerror("server address not specified");
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

int start_server(const char *listen_addr, const char *listen_port,
                 const char *dest_addr, const char *dest_port, const char *key)
{
    if (!key)
        argerror("encryption key not specified");
    if (!listen_addr)
        listen_addr = RTPTUN_DEFAULT_SERVER_LISTEN;
    if (!listen_port)
        listen_port = RTPTUN_DEFAULT_SERVER_PORT;
    if (!dest_addr)
        dest_addr = RTPTUN_DEFAULT_DEST_ADDR;
    if (!dest_port)
        argerror("destination port not specified");

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

int gen_key()
{
    char *key = chacha_gen_key();
    if (!key)
        return 1;

    puts(key);

    free(key);
    return 0;
}

void print_usage(FILE *out)
{
    static const char MESSAGE[] = "Usage: %1$s <action> <options>\n"
                                  "Example:\n"
                                  " - Generate key:     %1$s genkey\n"
                                  " - Run server:       %1$s server -k <KEY> -l 5004 -p 1194\n"
                                  " - Run client:       %1$s client -k <KEY> -l 1194 -d 192.0.2.1 -p 5004\n"
                                  " - Load config file: %1$s -f /etc/rtptun.conf\n"
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
                                  "  -k : encryption key\n"
                                  "\n"
                                  "Client options:\n"
                                  "  -i : local address (default: " RTPTUN_DEFAULT_CLIENT_LISTEN ")\n"
                                  "  -l : local port\n"
                                  "  -d : server address\n"
                                  "  -p : server port (default: " RTPTUN_DEFAULT_SERVER_PORT ")\n"
                                  "  -k : encryption key\n"
                                  "\n"
                                  "Program options:\n"
                                  "  -f : Load configuration file\n"
                                  "  -h : display help message\n"
                                  "  -v : verbose\n"
                                  "  -V : display version information\n";
    fprintf(out, MESSAGE, prog_name);
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

void argerror(const char *format, ...)
{
    fprintf(stderr, "%s: ", prog_name);

    va_list args;
    va_start(args, format);

    vfprintf(stderr, format, args);

    va_end(args);

    fputc('\n', stderr);
    fprintf(stderr, "Try '%s -h' for more information.\n", prog_name);

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