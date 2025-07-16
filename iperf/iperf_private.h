/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdatomic.h>
#include <inttypes.h>
#include <sys/queue.h>
#include "sdkconfig.h"
#include "esp_timer.h"
#include "esp_log.h"

#include "iperf_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#if __has_include("esp_check.h")
#include "esp_check.h"
#else
/* compatible with esp-idf v4.3 */
#define ESP_RETURN_ON_ERROR(x, log_tag, format, ...) do {                                       \
        esp_err_t err_rc_ = (x);                                                                \
        if (unlikely(err_rc_ != ESP_OK)) {                                                      \
            ESP_LOGE(log_tag, "%s(%d): " format, __FUNCTION__, __LINE__, ##__VA_ARGS__);        \
            return err_rc_;                                                                     \
        }                                                                                       \
    } while(0)

#define ESP_GOTO_ON_ERROR(x, goto_tag, log_tag, format, ...) do {                               \
        esp_err_t err_rc_ = (x);                                                                \
        if (unlikely(err_rc_ != ESP_OK)) {                                                      \
            ESP_LOGE(log_tag, "%s(%d): " format, __FUNCTION__, __LINE__, ##__VA_ARGS__);        \
            ret = err_rc_;                                                                      \
            goto goto_tag;                                                                      \
        }                                                                                       \
    } while(0)

#define ESP_GOTO_ON_FALSE(a, err_code, goto_tag, log_tag, format, ...) do {                                \
        if (unlikely(!(a))) {                                                                              \
            ESP_LOGE(log_tag, "%s(%d): " format, __FUNCTION__, __LINE__, ##__VA_ARGS__);                   \
            ret = err_code;                                                                                \
            goto goto_tag;                                                                                 \
        }                                                                                                  \
    } while (0)
#endif /* if __has_include("esp_check.h") */


/*************************************************
 * Definitions
 *************************************************/
#define TAG_ID_STR "iperf(id=%" PRIi8 ")"

/*************************************************
 * Structures
 *************************************************/
typedef struct {
    uint32_t period_data_snapshot;  /* max~4G bytes */
    uint32_t period_sec;
}
#if CONFIG_COMPILER_OPTIMIZATION_SIZE || CONFIG_COMPILER_OPTIMIZATION_PERF
__attribute__((aligned(8))) // IDF-12777
#endif
iperf_report_task_data_t;
// When -Os or -O2 is enabled, if 8-byte alignment is not explicitly enforced when defining the structure,
// using atomic_exchange on that structure type may lead to issues, and the value may not be exchanged correctly.

typedef struct {
    esp_timer_handle_t tx_timer;
    esp_timer_handle_t tick_timer;
    uint64_t tx_period_us;
    uint32_t ticks;
    uint32_t to_report_ticks;
} iperf_timers_t;

typedef struct {
    esp_ip_addr_t destination;
    esp_ip_addr_t source;
    struct sockaddr_storage target_addr; // Either the address to receive from if server, or send to if client
    uint16_t dport;
    uint16_t sport;
    int tos;  /* setsockopt does not accept uint8 size */
    uint8_t *buffer;
    uint32_t buffer_len;
} iperf_socket_info_t;

typedef struct iperf_instance_data_struct {
    LIST_ENTRY(iperf_instance_data_struct) _list_entry;

    iperf_id_t id;
    bool is_running;
    char tag[sizeof(TAG_ID_STR) + 1];
    uint32_t flags;

    TaskHandle_t report_task_hdl;
    TaskHandle_t traffic_task_hdl;

    iperf_timers_t timers;
    uint32_t interval;
    uint32_t time;

    int socket;
    iperf_socket_info_t socket_info;

    _Atomic uint32_t period_data_passed;
    _Atomic iperf_report_task_data_t report_task_data;
    iperf_traffic_report_t traffic;  /* traffic report, only modified by the report task to ensure thread safety */

    iperf_state_handler_func_t state_handler;
    void* state_handler_priv;
} iperf_instance_data_t;


/* internal used function */
void iperf_state_action(iperf_state_t iperf_state, iperf_instance_data_t *iperf_instance);
iperf_instance_data_t* iperf_list_get_instance_by_id(iperf_id_t id);
TaskHandle_t iperf_create_report_task(iperf_instance_data_t *iperf_instance);

#ifdef __cplusplus
}
#endif
