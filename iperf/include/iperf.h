/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_err.h"
#include "esp_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IPERF_IP_TYPE_IPV4 0
#define IPERF_IP_TYPE_IPV6 1
#define IPERF_TRANS_TYPE_TCP 0
#define IPERF_TRANS_TYPE_UDP 1

#define IPERF_FLAG_SET(cfg, flag) ((cfg) |= (flag))
#define IPERF_FLAG_CLR(cfg, flag) ((cfg) &= (~(flag)))

#define IPERF_FLAG_CLIENT (1)
#define IPERF_FLAG_SERVER (1 << 1)
#define IPERF_FLAG_TCP (1 << 2)
#define IPERF_FLAG_UDP (1 << 3)

#define IPERF_DEFAULT_PORT 5001
#define IPERF_DEFAULT_INTERVAL 3
#define IPERF_DEFAULT_TIME 30
#define IPERF_DEFAULT_NO_BW_LIMIT -1

#define IPERF_TRAFFIC_TASK_NAME "iperf_traffic"
#define IPERF_TRAFFIC_TASK_PRIORITY CONFIG_IPERF_TRAFFIC_TASK_PRIORITY
#define IPERF_TRAFFIC_TASK_STACK 4096
#define IPERF_REPORT_TASK_NAME "iperf_report"
#define IPERF_REPORT_TASK_PRIORITY CONFIG_IPERF_REPORT_TASK_PRIORITY
#define IPERF_REPORT_TASK_STACK 4096

#define IPERF_UDP_TX_LEN (1470)
#define IPERF_UDP_RX_LEN (16 << 10)
#define IPERF_TCP_TX_LEN (16 << 10)
#define IPERF_TCP_RX_LEN (16 << 10)

#define IPERF_MAX_DELAY 64

#define IPERF_SOCKET_RX_TIMEOUT CONFIG_IPERF_SOCKET_RX_TIMEOUT
#define IPERF_SOCKET_ACCEPT_TIMEOUT 5

#define IPERF_FORMAT_MBITS (0)
#define IPERF_FORMAT_KBITS (1)

extern bool g_iperf_is_running;

typedef struct {
    uint32_t flag;
    union {
        uint32_t destination_ip4;
        char    *destination_ip6;
    };
    union {
        uint32_t source_ip4;
        char    *source_ip6;
    };
    uint8_t type;
    uint8_t format;
    uint16_t dport;
    uint16_t sport;
    uint32_t interval;
    uint32_t time;
    uint16_t len_send_buf;
    int32_t bw_lim;
} iperf_cfg_t;

typedef enum {
    IPERF_TCP_SERVER,
    IPERF_TCP_CLIENT,
    IPERF_UDP_SERVER,
    IPERF_UDP_CLIENT,
} iperf_traffic_type_t;

typedef enum {
    IPERF_STARTED,
    IPERF_STOPPED,
} iperf_status_t;


/**
 * @brief Iperf traffic start with given config
 *
 * @param[in] cfg pointer to traffic config structure
 *
 * @return ESP_OK on success
 */
esp_err_t iperf_start(iperf_cfg_t *cfg);

/**
 * @brief Iperf traffic stop
 *
 * @return ESP_OK on success
 */
esp_err_t iperf_stop(void);


/* Support hook functions for performance debug */
typedef void (*iperf_hook_func_t)(iperf_traffic_type_t type, iperf_status_t status);
extern iperf_hook_func_t iperf_hook_func;

/**
 * @brief Registers iperf traffic start/stop hook function
 */
void app_register_iperf_hook_func(iperf_hook_func_t func);


#ifdef __cplusplus
}
#endif
