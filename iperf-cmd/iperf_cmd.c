/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
// #include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "iperf.h"

#include "esp_netif.h"

#ifndef APP_TAG
#define APP_TAG "IPERF"
#endif

typedef struct {
    struct arg_str *ip;
    struct arg_lit *server;
    struct arg_lit *udp;
    // struct arg_lit *version6;
    struct arg_int *port;
    struct arg_int *length;
    struct arg_int *interval;
    struct arg_int *time;
    struct arg_int *bw_limit;
    struct arg_str *format;
    struct arg_lit *abort;
    struct arg_end *end;
} iperf_args_t;
static iperf_args_t iperf_args;

static int cmd_do_iperf(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &iperf_args);
    iperf_cfg_t cfg;

    if (nerrors != 0) {
        arg_print_errors(stderr, iperf_args.end, argv[0]);
        return 0;
    }

    if (iperf_args.abort->count != 0) {
        iperf_stop();
        return 0;
    }

    if (g_iperf_is_running) {
        ESP_LOGE(APP_TAG, "iperf is already running.");
        return 1;
    }

    memset(&cfg, 0, sizeof(cfg));
    // now iperf-cmd only support IPV4 address
    cfg.type = IPERF_IP_TYPE_IPV4;

    if (((iperf_args.ip->count == 0) && (iperf_args.server->count == 0)) ||
            ((iperf_args.ip->count != 0) && (iperf_args.server->count != 0))) {
        ESP_LOGE(APP_TAG, "should specific client/server mode");
        return 0;
    }

    if (iperf_args.ip->count == 0) {
        cfg.flag |= IPERF_FLAG_SERVER;
    } else {
        cfg.destination_ip4 = esp_ip4addr_aton(iperf_args.ip->sval[0]);
        cfg.flag |= IPERF_FLAG_CLIENT;
    }

    // NOTE: Do not bind local ip now

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

    if (iperf_args.port->count == 0) {
        cfg.sport = IPERF_DEFAULT_PORT;
        cfg.dport = IPERF_DEFAULT_PORT;
    } else {
        if (cfg.flag & IPERF_FLAG_SERVER) {
            cfg.sport = iperf_args.port->ival[0];
            cfg.dport = IPERF_DEFAULT_PORT;
        } else {
            cfg.sport = IPERF_DEFAULT_PORT;
            cfg.dport = iperf_args.port->ival[0];
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
        cfg.bw_lim = iperf_args.bw_limit->ival[0];
        if (cfg.bw_lim <= 0) {
            cfg.bw_lim = IPERF_DEFAULT_NO_BW_LIMIT;
        }
    }
    /* -f --format */
    cfg.format = MBITS_PER_SEC;
    if (iperf_args.format->count > 0) {
        char format_ch = iperf_args.format->sval[0][0];
        if (format_ch == 'k') {
            cfg.format = KBITS_PER_SEC;
        } else if (format_ch == 'm') {
            cfg.format = MBITS_PER_SEC;
        } else {
            ESP_LOGW(APP_TAG, "ignore invalid format: %c", format_ch);
        }
    }

    ESP_LOGI(APP_TAG, "mode=%s-%s sip=%" PRId32 ".%" PRId32 ".%" PRId32 ".%" PRId32 ":%d,\
             dip=%" PRId32 ".%" PRId32 ".%" PRId32 ".%" PRId32 ":%d,\
             interval=%" PRId32 ", time=%" PRId32 "",
             cfg.flag & IPERF_FLAG_TCP ? "tcp" : "udp",
             cfg.flag & IPERF_FLAG_SERVER ? "server" : "client",
             cfg.source_ip4 & 0xFF, (cfg.source_ip4 >> 8) & 0xFF, (cfg.source_ip4 >> 16) & 0xFF,
             (cfg.source_ip4 >> 24) & 0xFF, cfg.sport,
             cfg.destination_ip4 & 0xFF, (cfg.destination_ip4 >> 8) & 0xFF,
             (cfg.destination_ip4 >> 16) & 0xFF, (cfg.destination_ip4 >> 24) & 0xFF, cfg.dport,
             cfg.interval, cfg.time);

    iperf_start(&cfg);

    return 0;
}

esp_err_t app_register_iperf_commands(void)
{
    /* same with official iperf: https://iperf.fr/iperf-doc.php */
    iperf_args.ip = arg_str0("c", "client", "<host>", "run in client mode, connecting to <host>");
    iperf_args.server = arg_lit0("s", "server", "run in server mode");
    iperf_args.udp = arg_lit0("u", "udp", "use UDP rather than TCP");
// #ifdef CONFIG_LWIP_IPV6
//     iperf_args.version6 = arg_lit0("6", "version6", "Use IPv6 addresses");
// #endif
    iperf_args.port = arg_int0("p", "port", "<port>", "server port to listen on/connect to");
    iperf_args.length = arg_int0("l", "len", "<length>", "Set read/write buffer size");
    iperf_args.interval = arg_int0("i", "interval", "<interval>", "seconds between periodic bandwidth reports");
    iperf_args.time = arg_int0("t", "time", "<time>", "time in seconds to transmit for (default 10 secs)");
    iperf_args.bw_limit = arg_int0("b", "bandwidth", "<bandwidth>", "bandwidth to send at in Mbits/sec");
    iperf_args.format = arg_str0("f", "format", "<format>", "'k' = Kbits/sec 'm' = Mbits/sec");
    /* abort is not an official option */
    iperf_args.abort = arg_lit0(NULL, "abort", "abort running iperf");
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
