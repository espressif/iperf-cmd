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
#define IPERF_FLAG_REPORT_NO_PRINT  BIT(4)

#define IPERF_DEFAULT_PORT          5001
#define IPERF_DEFAULT_INTERVAL      3
#define IPERF_DEFAULT_TIME          30
#define IPERF_DEFAULT_NO_BW_LIMIT   -1

#define IPERF_TRAFFIC_TASK_NAME "iperf_traffic"
#define IPERF_DEFAULT_TRAFFIC_TASK_PRIORITY CONFIG_IPERF_DEF_TRAFFIC_TASK_PRIORITY
#define IPERF_TRAFFIC_TASK_STACK 4096
#define IPERF_REPORT_TASK_NAME "iperf_report"
#define IPERF_REPORT_TASK_PRIORITY CONFIG_IPERF_DEF_REPORT_TASK_PRIORITY
#define IPERF_REPORT_TASK_STACK 4096

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
 * @brief Iperf output report format
 */
typedef enum {
    MBITS_PER_SEC,
    KBITS_PER_SEC,
    BITS_PER_SEC,
} iperf_output_format_t;

/**
 * @brief Iperf traffic type
 */
typedef enum {
    IPERF_TCP_SERVER,
    IPERF_TCP_CLIENT,
    IPERF_UDP_SERVER,
    IPERF_UDP_CLIENT,
} iperf_traffic_type_t;

/**
 * @brief Iperf status
 */
typedef enum {
    IPERF_STARTED,  /**< iperf started sending/receiving data */
    IPERF_STOPPED,  /**< iperf stopped sending/receiving data */
    IPERF_RUNNING,  /**< iperf is running and periodic report interval exceeded */
    IPERF_CLOSED    /**< iperf instance finished its execution and is being closed */
} iperf_state_t;

/**
 * @brief Iperf IP type
 */
typedef enum {
    IPERF_IP_TYPE_IPV4,
    IPERF_IP_TYPE_IPV6
} iperf_ip_type_t;

/**
 * @brief iperf instance ID
 */
typedef int8_t iperf_id_t;

/**
 * @brief Structure of common data present in any iperf report
 *
 */
typedef struct {
    iperf_output_format_t output_format;  /**< output format, Mbits or Kbits */
    float period_bandwidth;  /**< bandwidth during current interval */
    float period_transfer;  /**< data transferred during current interval */
    float average_bandwidth;  /**< average bandwidth from start to end */
    double curr_total_transfer;  /**< current data transferred since iperf has started */
    uint32_t current_time_sec;  /**< current time since iperf has started */
} iperf_report_t;

/**
 * @brief iperf report handler function to receive runtime details from iperf
 */
typedef void (*iperf_report_handler_func_t)(iperf_id_t id, iperf_state_t iperf_state, void *priv);

/**
 * @brief Iperf Configuration
 */
typedef struct {
    esp_ip_addr_t destination; /**< destination IP */
    esp_ip_addr_t source; /**< source IP */
    iperf_output_format_t format;  /**< output format, bits/sec, Kbits/sec, Mbits/sec */
    iperf_report_handler_func_t report_handler;  /**< iperf status report function */
    void *report_handler_priv; /**< pointer to user's private data later passed to report function */
    uint32_t flag; /**< iperf flag */
    uint32_t interval;  /**< report interval in secs */
    uint32_t time;  /**< total send time in secs */
    int32_t bw_lim;  /**< bandwidth limit in bits/s */
    uint16_t dport;  /**< destination port */
    uint16_t sport;  /**< source port */
    uint16_t len_send_buf;  /**< send buffer length in bytes */
    int tos;  /**< set socket TOS field */
    uint8_t traffic_task_priority;  /**< iperf traffic task priority */
    iperf_id_t instance_id;  /**< iperf instance id */
} iperf_cfg_t;

/**
 * @brief Set report handler during runtime for instance with given id
 *
 * @note This function is not thread safe. Recommended to be used only inside `report_handler` callback.
 *
 * @param id iperf instance ID
 * @param handler function pointer to the handler
 * @param priv pointer to user's private data later passed to the report function
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if invalid argument
 *      - ESP_ERR_INVALID_STATE if instance with associated ID was not found
 */
esp_err_t iperf_register_report_handler(iperf_id_t id, iperf_report_handler_func_t handler, void *priv);

/**
 * @brief Get the current performance report of instance.
 *
 * @warning Not thread safe. Recommended to be used only inside `report_handler` callback
 *          and only when iperf state is `IPERF_RUNNING` or `IPERF_CLOSED`.
 *
 * @param id iperf instance ID
 * @param report pointer where to copy the report
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if invalid argument
 *      - ESP_ERR_INVALID_STATE if instance with associated ID was not found
 */
esp_err_t iperf_get_report(iperf_id_t id, iperf_report_t *report);


/**
 * @brief Get the traffic type of instance.
 *
 * @note Not thread safe. Recommended to be used only inside `report_handler` callback.
 *
 * @param id iperf instance ID
 * @param traffic_type pointer where to store traffic type
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if invalid argument
 *      - ESP_ERR_INVALID_STATE if instance with associated ID was not found
 */
esp_err_t iperf_get_traffic_type(iperf_id_t id, iperf_traffic_type_t *traffic_type);

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
