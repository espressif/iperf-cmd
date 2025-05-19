/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <sys/socket.h>
// #include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "esp_netif.h"

#include "iperf.h"


#ifndef APP_TAG
#define APP_TAG "IPERF"
#endif

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#define IPERF_CMD_INSTANCE_USE_IPV4    1
#define IPERF_CMD_INSTANCE_USE_IPV6    2

typedef struct {
    struct arg_lit *help;
    struct arg_str *ip;
    struct arg_lit *server;
    struct arg_lit *udp;
#if IPERF_IPV6_ENABLED
    struct arg_lit *ipv6_domain;
#endif
    struct arg_int *port;
    struct arg_str *bind;
    struct arg_int *cport;
    struct arg_int *length;
    struct arg_int *interval;
    struct arg_int *time;
    struct arg_str *bw_limit;
    struct arg_str *format;
    struct arg_int *tos;
    struct arg_int *id;
    struct arg_lit *abort;
    struct arg_int *parallel;
    struct arg_end *end;
} iperf_args_t;

static iperf_args_t iperf_args;
static iperf_report_handler_func_t s_report_hndl;
static void *s_report_priv;

static int32_t iperf_bandwidth_convert(const char *bandwidth_str)
{
    int len = strlen(bandwidth_str);
    if (len == 0) {
        return IPERF_DEFAULT_NO_BW_LIMIT;
    }

    char *endptr = NULL;
    int32_t value = strtol(bandwidth_str, &endptr, 10);
    if (value <= 0) {
        return IPERF_DEFAULT_NO_BW_LIMIT;
    }

    int base = 1;
    switch (endptr[0])
    {
    case 'k':
        base = 1000;
        break;
    case 'K':
        base = 1024;
        break;
    case 'm':
        base = 1000 * 1000;
        break;
    case 'M':
        base = 1024 * 1024;
        break;
    case 'g':
        base = 1000 * 1000 * 1000;
        break;
    case 'G':
        base = 1024 * 1024 * 1024;
        break;
    default:
#if CONFIG_IPERF_CMD_DEFAULT_BW_MBPS
        base = 1000 * 1000;
#endif
        break;
    }

    value = value * base;
    if (value <= 0) {
        return IPERF_DEFAULT_NO_BW_LIMIT;
    } else {
        return value;
    }
}

static int cmd_do_iperf(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &iperf_args);
    int error = 0;
    iperf_cfg_t cfg;

    if (nerrors != 0) {
        arg_print_errors(stderr, iperf_args.end, argv[0]);
        return 1;
    }

    if (iperf_args.help->count != 0) {
        printf("Usage: iperf");
        arg_print_syntax(stdout, (void *) &iperf_args, "\n\n");
        arg_print_glossary(stdout, (void *) &iperf_args, "  %-30s %s\n");
        return 0;
    }

    if (iperf_args.abort->count != 0) {
        if(iperf_args.id->count == 0) {
            iperf_stop_instance(IPERF_ALL_INSTANCES_ID);
        } else {
            iperf_stop_instance(iperf_args.id->ival[0]);
        }
        return 0;
    }

    memset(&cfg, 0, sizeof(cfg));

    /* Given instance id */
    if(iperf_args.id->count != 0) {
        cfg.instance_id = (iperf_id_t)iperf_args.id->ival[0];
        if (cfg.instance_id <= 0) {
            ESP_LOGE(APP_TAG, "Invalid iperf ID");
            return 1;
        }
    }

    uint8_t instance_type = IPERF_CMD_INSTANCE_USE_IPV4;
#if IPERF_IPV6_ENABLED
    if (iperf_args.ipv6_domain->count > 0) {
        instance_type = IPERF_CMD_INSTANCE_USE_IPV6;
        cfg.source.type = ESP_IPADDR_TYPE_V6;
        cfg.destination.type = ESP_IPADDR_TYPE_V6;
    }
#endif

    if (((iperf_args.ip->count == 0) && (iperf_args.server->count == 0)) ||
            ((iperf_args.ip->count != 0) && (iperf_args.server->count != 0))) {
        ESP_LOGE(APP_TAG, "should specific client/server mode");
        return 0;
    }

    if (iperf_args.ip->count == 0) {
        cfg.flag |= IPERF_FLAG_SERVER;
    } else {
        cfg.flag |= IPERF_FLAG_CLIENT;
#if IPERF_IPV6_ENABLED
        if (instance_type == IPERF_CMD_INSTANCE_USE_IPV6) {
            error = inet_pton(AF_INET6, (char*)(iperf_args.ip->sval[0]), &cfg.destination.u_addr.ip6.addr);
        }
#endif
#if IPERF_IPV4_ENABLED
        if (instance_type == IPERF_CMD_INSTANCE_USE_IPV4) {
            error = inet_pton(AF_INET, (char*)(iperf_args.ip->sval[0]), &cfg.destination.u_addr.ip4.addr);
        }
#endif
        if (error == 0) {
            ESP_LOGE(APP_TAG, "invalid destination address");
            return 1;
        }
    }

    if (iperf_args.udp->count == 0) {
        cfg.flag |= IPERF_FLAG_TCP;
    } else {
        cfg.flag |= IPERF_FLAG_UDP;
    }

    if (iperf_args.length->count == 0) {
        cfg.len_send_buf = 0;
    } else {
        cfg.len_send_buf = iperf_args.length->ival[0];
    }

    if (iperf_args.bind->count != 0) {
#if IPERF_IPV6_ENABLED
        if (instance_type == IPERF_CMD_INSTANCE_USE_IPV6) {
            error = inet_pton(AF_INET6, (char*)(iperf_args.bind->sval[0]), &cfg.source.u_addr.ip6.addr);
        }
#endif
#if IPERF_IPV4_ENABLED
        if (instance_type == IPERF_CMD_INSTANCE_USE_IPV4) {
            error = inet_pton(AF_INET, (char*)(iperf_args.bind->sval[0]), &cfg.source.u_addr.ip4.addr);
        }
#endif
        if (error == 0) {
            ESP_LOGE(APP_TAG, "invalid address to bind");
            return 1;
        }
    }

    if (cfg.flag & IPERF_FLAG_SERVER) {
        if (iperf_args.port->count == 0) {
            cfg.sport = IPERF_DEFAULT_PORT;
        } else {
            cfg.sport = iperf_args.port->ival[0];
        }
    } else { // IPERF_FLAG_CLIENT
        if (iperf_args.port->count == 0) {
            cfg.dport = IPERF_DEFAULT_PORT;
        } else {
            cfg.dport = iperf_args.port->ival[0];
        }
        if (iperf_args.cport->count != 0) {
            cfg.sport = iperf_args.cport->ival[0];
        }
    }

    if (iperf_args.interval->count == 0) {
        cfg.interval = IPERF_DEFAULT_INTERVAL;
    } else {
        cfg.interval = iperf_args.interval->ival[0];
        if (cfg.interval <= 0) {
            cfg.interval = IPERF_DEFAULT_INTERVAL;
        }
    }
    if (iperf_args.time->count == 0) {
        cfg.time = IPERF_DEFAULT_TIME;
    } else {
        cfg.time = iperf_args.time->ival[0];
        if (cfg.time <= cfg.interval) {
            cfg.time = cfg.interval;
        }
    }
    /* iperf -b */
    if (iperf_args.bw_limit->count == 0) {
        cfg.bw_lim = IPERF_DEFAULT_NO_BW_LIMIT;
    } else {
        cfg.bw_lim = iperf_bandwidth_convert(iperf_args.bw_limit->sval[0]);
    }
    /* -f --format */
    cfg.format = MBITS_PER_SEC;
    if (iperf_args.format->count > 0) {
        char format_ch = iperf_args.format->sval[0][0];
        if (format_ch == 'b') {
            cfg.format = BITS_PER_SEC;
        } else if (format_ch == 'k') {
            cfg.format = KBITS_PER_SEC;
        } else if (format_ch == 'm') {
            cfg.format = MBITS_PER_SEC;
        } else {
            ESP_LOGW(APP_TAG, "ignore invalid format: %c", format_ch);
        }
    }
    /* iperf --tos */
    if (iperf_args.tos->count > 0) {
        cfg.tos = iperf_args.tos->ival[0];
    }

    cfg.report_handler = s_report_hndl;
    cfg.report_handler_priv = s_report_priv;

#if IPERF_IPV6_ENABLED
    char dip_str[INET6_ADDRSTRLEN];
    char sip_str[INET6_ADDRSTRLEN];
#elif IPERF_IPV4_ENABLED
    char dip_str[INET_ADDRSTRLEN];
    char sip_str[INET_ADDRSTRLEN];
#endif
#if IPERF_IPV6_ENABLED
    if (instance_type == IPERF_CMD_INSTANCE_USE_IPV6) {
        inet_ntop(AF_INET6, &cfg.destination.u_addr.ip6.addr, dip_str, sizeof(dip_str));
        inet_ntop(AF_INET6, &cfg.source.u_addr.ip6.addr, sip_str, sizeof(sip_str));
    }
#endif
#if IPERF_IPV4_ENABLED
    if (instance_type == IPERF_CMD_INSTANCE_USE_IPV4) {
        inet_ntop(AF_INET, &cfg.destination.u_addr.ip4.addr, dip_str, sizeof(dip_str));
        inet_ntop(AF_INET, &cfg.source.u_addr.ip4.addr, sip_str, sizeof(sip_str));
    }
#endif

    // Following the convention IPv6 address must be in square brackets if followed by a port
    // aaaa:bbbb:cccc:dddd:eeee:ffff:gggg:hhhh:PORT - wrong
    // [aaaa:bbbb:cccc:dddd:eeee:ffff:gggg:hhhh]:PORT - right
    ESP_LOGI(APP_TAG, "mode=%s-%s sip=%s%s%s:%" PRId32 ", dip=%s%s%s:%" PRId32 ", interval=%" PRId32 ", time=%" PRId32,
             cfg.flag & IPERF_FLAG_TCP ? "tcp" : "udp",
             cfg.flag & IPERF_FLAG_SERVER ? "server" : "client",
             instance_type == IPERF_CMD_INSTANCE_USE_IPV6 ? "[" : "",
             sip_str,
             instance_type == IPERF_CMD_INSTANCE_USE_IPV6 ? "]" : "",
             cfg.sport,
             instance_type == IPERF_CMD_INSTANCE_USE_IPV6 ? "[" : "",
             dip_str,
             instance_type == IPERF_CMD_INSTANCE_USE_IPV6 ? "]" : "",
             cfg.dport,
             cfg.interval, cfg.time);

    /* parallel*/
    int parallel = 1;
    if (iperf_args.parallel->count > 0) {
        if ((iperf_args.server->count > 0) || (iperf_args.id->count > 0)) {
            ESP_LOGE(APP_TAG, "parallel option should not be used with server mode or specific instance id");
            return 1;
        }
        parallel = iperf_args.parallel->ival[0];
        if (parallel < 1 || parallel > IPERF_SOCKET_MAX_NUM) {
            ESP_LOGE(APP_TAG, "invalid parallel number");
            return 1;
        }
    }
    for (int i = 0; i < parallel; i++) {
        iperf_start_instance(&cfg);
    }

    return 0;
}

esp_err_t iperf_cmd_register_report_handler(iperf_report_handler_func_t report_hndl, void *priv)
{
    s_report_hndl = report_hndl;
    s_report_priv = priv;
    return ESP_OK;
}

esp_err_t iperf_cmd_register_iperf(void)
{
    /* same with official iperf: https://iperf.fr/iperf-doc.php */
    iperf_args.help = arg_lit0(NULL, "help", "display this help and exit");
    iperf_args.ip = arg_str0("c", "client", "<host>", "run in client mode, connecting to <host>");
    iperf_args.server = arg_lit0("s", "server", "run in server mode");
    iperf_args.udp = arg_lit0("u", "udp", "use UDP rather than TCP");
#if IPERF_IPV6_ENABLED
    /*
     * NOTE: iperf2 uses -V(--ipv6_domain or --IPv6Version) for ipv6
     * iperf3 uses -6(--version6)
     * May add a new command "iperf3" in the future
     */
    iperf_args.ipv6_domain = arg_lit0("V", "ipv6_domain", "Set the domain to IPv6 (send packets over IPv6)");
#endif
    iperf_args.port = arg_int0("p", "port", "<port>", "server port to listen on/connect to");
    iperf_args.bind = arg_str0("B", "bind", "<host>", "bind to interface at <host> address");
    iperf_args.cport = arg_int0(NULL, "cport", "<port>", "bind to a specific client port");
    iperf_args.length = arg_int0("l", "len", "<length>", "length of buffer in bytes to write"
                                                         "(Defaults: TCP=" STR(IPERF_DEFAULT_TCP_TX_LEN)
                                                         ", IPv4 UDP=" STR(IPERF_DEFAULT_IPV4_UDP_TX_LEN)
                                                         ", IPv6 UDP=" STR(IPERF_DEFAULT_IPV6_UDP_TX_LEN) ")");
    iperf_args.interval = arg_int0("i", "interval", "<interval>", "seconds between periodic bandwidth reports");
    iperf_args.time = arg_int0("t", "time", "<time>", "time in seconds to transmit for (default 10 secs)");
    iperf_args.bw_limit = arg_str0("b", "bandwidth", "<bandwidth>", "#[kmgKMG]  bandwidth to send at in bits/sec");
    iperf_args.format = arg_str0("f", "format", "<format>", "'b' = bits/sec 'k' = Kbits/sec 'm' = Mbits/sec");
    iperf_args.tos = arg_int0("S", "tos", "<tos>", "set the socket's IP_TOS (byte) field");
    /* iperf instance id */
    iperf_args.id = arg_int0(NULL, "id", "<id>", "iperf instance ID. default: 'increase' for create, 'all' for abort.");
    /* abort is not an official option */
    iperf_args.abort = arg_lit0(NULL, "abort", "abort running iperf");
    iperf_args.parallel = arg_int0("P", "parallel", "<parallel number>", "number of parallel client threads to run");
    iperf_args.end = arg_end(1);
    const esp_console_cmd_t iperf_cmd = {
        .command = "iperf",
        .help = "iperf command to measure network performance, through TCP or UDP connections.",
        .hint = NULL,
        .func = &cmd_do_iperf,
        .argtable = &iperf_args
    };
    return esp_console_cmd_register(&iperf_cmd);
}
