/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IPERF_WEAK_ATTR __attribute__((weak))

/**
 * @brief Iperf output report format
 */
 typedef enum {
    MBYTES_PER_SEC,
    KBYTES_PER_SEC,
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
    IPERF_TRAFFIC_TYPE_INVALID,
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
 * @brief iperf report type
 */
typedef enum {
    IPERF_REPORT_CONNECT_INFO,  /**< iperf report connected info */
    IPERF_REPORT_PERIOD,  /**< iperf report period traffic */
    IPERF_REPORT_SUMMARY  /**< iperf report traffic summary */
} iperf_report_type_t;

/**
 * @brief Structure of data for iperf report traffic data
 *
 */
 typedef struct {
    uint32_t period_start_sec;  /**< period start time since iperf has started */
    uint32_t end_sec;  /**< report data end time since iperf has started */
    double period_bytes;  /**< data transferred in bytes within this period */
    iperf_output_format_t output_format;  /**< output format, bits/sec, Kbits/sec, Mbits/sec */
    uint64_t total_transfer_bytes;  /**< total bytes transferred since iperf has started */
} iperf_traffic_report_t;

/**
 * @brief Structure of data for iperf report traffic data
 *
 */
typedef struct {
    int socket; /**< socket id */
    struct sockaddr_storage target_addr;  /**< Either the address to receive from if server, or send to if client */
} iperf_connect_info_report_t;

/**
 * @brief Structure of data for iperf report
 *
 */
 typedef struct {
    iperf_id_t instance_id;  /**< iperf instance id */
    iperf_report_type_t report_type;  /**< iperf report type */
    union {
        iperf_traffic_report_t traffic;  /**< traffic report for report type: PERIOD and SUMMARY */
        iperf_connect_info_report_t connect_info;  /**< connect info report for report type: PCONNECT_INFO */
    };
} iperf_report_t;

/**
 * @brief Structure of data for iperf state handler
 */
 typedef struct {
    iperf_state_t state;  /**< iperf state */
    iperf_traffic_type_t traffic_type;  /**< iperf traffic iperf */
} iperf_state_data_t;

/**
 * @brief function to handle iperf state transitions
 */
typedef void (*iperf_state_handler_func_t)(iperf_id_t instance_id, iperf_state_data_t* data, void* priv);

 /**
  * @brief Iperf Configuration
  */
 typedef struct {
     esp_ip_addr_t destination; /**< destination IP */
     esp_ip_addr_t source; /**< source IP */
     iperf_output_format_t format;  /**< output format, bits/sec, Kbits/sec, Mbits/sec */
     iperf_state_handler_func_t state_handler;  /**< iperf state handler function */
     void *state_handler_priv; /**< pointer to user's private data later passed to state_handler function */
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


#ifdef __cplusplus
}
#endif
