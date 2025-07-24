/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include "esp_err.h"
#include "esp_types.h"
#include "esp_netif_ip_addr.h"
#include "esp_bit_defs.h"
#include "sdkconfig.h"
#include "iperf_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * There's no CONFIG_LWIP_IPV4 in idf<5.1
 * use LWIP_IPV4 from lwipopts.h (sys/socket.h)
 * LWIP_IPV4 should always be defined (0/1).
 */
#ifndef LWIP_IPV4
#error "LWIP_IPV4 should be defined from lwipopts.h (sys/socket.h)."
#endif

/*
 * We only use lwip stack for now, but we may support different IP stack in the future.
 */
#define IPERF_IPV4_ENABLED LWIP_IPV4
#define IPERF_IPV6_ENABLED LWIP_IPV6

#define IPERF_FLAG_SET(cfg, flag) ((cfg) |= (flag))
#define IPERF_FLAG_CLR(cfg, flag) ((cfg) &= (~(flag)))

#define IPERF_FLAG_CLIENT           BIT(0)
#define IPERF_FLAG_SERVER           BIT(1)
#define IPERF_FLAG_TCP              BIT(2)
#define IPERF_FLAG_UDP              BIT(3)

#define IPERF_DEFAULT_PORT          5001
#define IPERF_DEFAULT_INTERVAL      3
#define IPERF_DEFAULT_TIME          30
#define IPERF_DEFAULT_NO_BW_LIMIT   -1

#define IPERF_TRAFFIC_TASK_NAME "iperf_traffic"
#define IPERF_DEFAULT_TRAFFIC_TASK_PRIORITY CONFIG_IPERF_DEF_TRAFFIC_TASK_PRIORITY
#define IPERF_TRAFFIC_TASK_STACK CONFIG_IPERF_DEF_TRAFFIC_TASK_STACK
#define IPERF_REPORT_TASK_NAME "iperf_report"
#define IPERF_REPORT_TASK_PRIORITY CONFIG_IPERF_DEF_REPORT_TASK_PRIORITY
#define IPERF_REPORT_TASK_STACK CONFIG_IPERF_DEF_REPORT_TASK_STACK

#define IPERF_DEFAULT_IPV4_UDP_TX_LEN   CONFIG_IPERF_DEF_IPV4_UDP_TX_BUFFER_LEN
#define IPERF_DEFAULT_IPV6_UDP_TX_LEN   CONFIG_IPERF_DEF_IPV6_UDP_TX_BUFFER_LEN
#define IPERF_DEFAULT_UDP_RX_LEN        CONFIG_IPERF_DEF_UDP_RX_BUFFER_LEN
// TODO: change default value on v1.0, 16 << 10 may be too big for some sdk with small RAM
#define IPERF_DEFAULT_TCP_TX_LEN        CONFIG_IPERF_DEF_TCP_TX_BUFFER_LEN
#define IPERF_DEFAULT_TCP_RX_LEN        CONFIG_IPERF_DEF_TCP_RX_BUFFER_LEN

#define IPERF_SOCKET_RX_TIMEOUT         CONFIG_IPERF_DEF_SOCKET_RX_TIMEOUT
#define IPERF_SOCKET_TCP_TX_TIMEOUT     CONFIG_IPERF_DEF_SOCKET_TCP_TX_TIMEOUT
#define IPERF_SOCKET_ACCEPT_TIMEOUT     5
#define IPERF_SOCKET_MAX_NUM            CONFIG_LWIP_MAX_SOCKETS

/**
 * @brief Default config to run iperf in client mode
 *
 * @param[in] proto protocol flag (TCP/UDP)
 * @param[in] ip    `esp_ip_addr_t` value of source ip
 */
#define IPERF_DEFAULT_CONFIG_CLIENT(proto, ip)  {                                           \
                                                    .flag = proto | IPERF_FLAG_CLIENT,      \
                                                    .format = MBITS_PER_SEC,                \
                                                    .interval = IPERF_DEFAULT_INTERVAL,     \
                                                    .time = IPERF_DEFAULT_TIME,             \
                                                    .bw_lim = IPERF_DEFAULT_NO_BW_LIMIT,    \
                                                    .destination = ip,                      \
                                                    .dport = IPERF_DEFAULT_PORT             \
                                                }

/**
 * @brief Default config to run iperf in server mode
 *
 * @param[in] proto protocol flag (TCP/UDP)
 * @param[in] ip    `esp_ip_addr_t` value of destination ip
 */
#define IPERF_DEFAULT_CONFIG_SERVER(proto, ip)  {                                           \
                                                    .flag = proto | IPERF_FLAG_SERVER,      \
                                                    .format = MBITS_PER_SEC,                \
                                                    .interval = IPERF_DEFAULT_INTERVAL,     \
                                                    .time = IPERF_DEFAULT_TIME,             \
                                                    .bw_lim = IPERF_DEFAULT_NO_BW_LIMIT,    \
                                                    .source = ip,                           \
                                                    .sport = IPERF_DEFAULT_PORT             \
                                                }

/**
 * @brief Special ID used to perform actions on all running instances.
 *
 * This macro can be passed to APIs that consume instance IDs to indicate
 * that the action should apply to all running application instances.
 *
 * @note Not all APIs support this special ID. Refer to the specific API
 * documentation to determine whether this feature is supported.
 *
 */
#define IPERF_ALL_INSTANCES_ID              (-1)

/**
 * @brief Get the current performance report of instance.
 *
 * @warning Not thread safe. Recommended to be used only inside `state_handler` callback
 *          and only when iperf state is `IPERF_RUNNING` or `IPERF_CLOSED`.
 *
 * @param id iperf instance ID
 * @param report pointer where to copy the report
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if invalid argument
 *      - ESP_ERR_INVALID_STATE if instance with associated ID was not found
 */
esp_err_t iperf_get_traffic_report(iperf_id_t id, iperf_traffic_report_t *report);

/**
 * @brief Iperf default report output (print) function.
 *
 * @param report iperf report data
 */
void iperf_default_report_output(const iperf_report_t* report);

/**
* @brief Iperf report output, defaults to `iperf_default_report_print`
*
* Outputs bandwidth statistics in the Iperf format, e.g.:
* "[  3]  3.0- 4.0 sec   128 KBytes  1.05 Mbits/sec".
*
* @note This function is set to "weak", allowing users to override it with a custom implementation.
*
* @param report iperf traffic report data
*/
IPERF_WEAK_ATTR void iperf_report_output(const iperf_report_t* report);

/**
 * @brief Start iperf instance
 *
 * @param[in] cfg pointer to traffic config structure
 *
 * @return
 *      - iperf instance ID
 *      - -1 if couldn't start new instance
 */
iperf_id_t iperf_start_instance(const iperf_cfg_t *cfg);

/**
 * @brief Stop iperf instance
 *
 * @param[in] id iperf instance ID. Setting to `IPERF_ALL_INSTANCES_ID` stops all running instances
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_FAIL if instance with given id was not found
 *      - ESP_ERR_INVALID_ARG if invalid id is provided (id < 0)
 */
esp_err_t iperf_stop_instance(iperf_id_t id);


#ifdef __cplusplus
}
#endif
