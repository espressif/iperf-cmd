/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <sys/socket.h>
#include <stdatomic.h>
#include <sys/queue.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <errno.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "iperf.h"

static const char *TAG = "iperf";
#define TAG_ID_STR "iperf(id=%" PRIi8 ")"

#ifdef CONFIG_FREERTOS_NUMBER_OF_CORES
/* new in idf v5.3 */
#define NUMBER_OF_CORES CONFIG_FREERTOS_NUMBER_OF_CORES
#else
#define NUMBER_OF_CORES portNUM_PROCESSORS
#endif

#define IPERF_TICK_US                       (1000000)
#define IPERF_TIME_FORCE_ELAPSED            (0)
#define IPERF_TASKS_FINISH_TMO_MS           (1500)
#define IPERF_LIST_LOCK_TMO_RTOS_TICKS      portMAX_DELAY

#define TAG_ID iperf_instance->tag

typedef struct {
    uint32_t period_data_snapshot;
    uint32_t report_interval_sec;
} iperf_report_task_t;

typedef struct {
    float val;
    uint32_t k;
} iperf_running_average_t;

typedef struct {
    iperf_output_format_t format;
    float period_bandwidth;
    float period_transfer;
    iperf_running_average_t average_bandwidth;
    double curr_total_transfer;
    uint32_t prev_time_sec;
    uint32_t current_time_sec;
} iperf_stats_t;

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
    uint8_t *buffer;
    uint32_t buffer_len;
} iperf_socket_info_t;

typedef enum {
    IPERF_PRINT_CONNECT_INFO,
    IPERF_PRINT_STATS,
    IPERF_PRINT_SUMMARY
} iperf_print_type_t;

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
    _Atomic iperf_report_task_t report_task_data;
    iperf_stats_t stats;

    iperf_report_handler_func_t report_handler;
    void *report_handler_priv;
} iperf_instance_data_t;

typedef esp_err_t (*iperf_list_exec_fn)(iperf_instance_data_t *iperf_instance, void *ctx);

DRAM_ATTR static LIST_HEAD(iperfs_list, iperf_instance_data_struct) running_iperfs_list = LIST_HEAD_INITIALIZER(running_iperfs_list);
static struct iperfs_list *p_running_iperfs_list = &running_iperfs_list;

static StaticSemaphore_t s_list_lock_buffer; // Static storage for the mutex
static SemaphoreHandle_t s_list_lock = NULL;

static esp_err_t iperf_stop_exec(iperf_instance_data_t *iperf_instance);
static void iperf_delete_instance(iperf_instance_data_t *iperf_instance);

static void iperf_print_report(iperf_print_type_t print_type, iperf_instance_data_t *iperf_instance)
{
    if ((iperf_instance->flags & IPERF_FLAG_REPORT_NO_PRINT) == IPERF_FLAG_REPORT_NO_PRINT) {
        return;
    }
    char format_ch = iperf_instance->stats.format == MBITS_PER_SEC ? 'M' : 'K';
    switch (print_type) {
    case IPERF_PRINT_CONNECT_INFO: {
        if (iperf_instance->socket_info.target_addr.ss_family == AF_INET) {
#if IPERF_IPV4_ENABLED
            struct sockaddr_in local_addr4 = { 0 };
            socklen_t addr4_len = sizeof(local_addr4);
            if (getsockname(iperf_instance->socket, (struct sockaddr *)&local_addr4, &addr4_len) == 0) {
                char localaddr_str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &local_addr4.sin_addr.s_addr, localaddr_str, sizeof(localaddr_str));
                char targetaddr_str[INET_ADDRSTRLEN];
                struct sockaddr_in *target_addr4 = (struct sockaddr_in *)&iperf_instance->socket_info.target_addr;
                inet_ntop(AF_INET, &target_addr4->sin_addr.s_addr, targetaddr_str, sizeof(targetaddr_str));
                printf("[%3d] local %s:%" PRIu16 " connected to %s:%" PRIu16 "\n",
                       iperf_instance->id,
                       localaddr_str, ntohs(local_addr4.sin_port),
                       targetaddr_str, ntohs(target_addr4->sin_port));
            }
#endif
        } else if (iperf_instance->socket_info.target_addr.ss_family == AF_INET6) {
#if IPERF_IPV6_ENABLED
            struct sockaddr_in6 local_addr6 = { 0 };
            socklen_t addr6_len = sizeof(local_addr6);
            if (getsockname(iperf_instance->socket, (struct sockaddr *)&local_addr6, &addr6_len) == 0) {
                char localaddr_str[INET6_ADDRSTRLEN];
                inet_ntop(AF_INET6, &local_addr6.sin6_addr, localaddr_str, sizeof(localaddr_str));
                char targetaddr_str[INET6_ADDRSTRLEN];
                struct sockaddr_in6 *target_addr6 = (struct sockaddr_in6 *)&iperf_instance->socket_info.target_addr;
                inet_ntop(AF_INET6, &target_addr6->sin6_addr, targetaddr_str, sizeof(targetaddr_str));
                printf("[%3d] local [%s]:%" PRIu16 " connected to [%s]:%" PRIu16 "\n",
                       iperf_instance->id,
                       localaddr_str, ntohs(local_addr6.sin6_port),
                       targetaddr_str, ntohs(target_addr6->sin6_port));
            }
#endif
        }
        // only the first instance prints the header
        if (iperf_instance->id == 0) {
            printf("\n[ ID] Interval\t\tTransfer\tBandwidth\n");
        }
        break;
    }
    case IPERF_PRINT_STATS:
        printf("[%3d] %2" PRIu32 ".0-%2" PRIu32 ".0 sec\t%2.2f %cBytes\t%.2f %cbits/sec\n", iperf_instance->id,
               iperf_instance->stats.prev_time_sec,
               iperf_instance->stats.current_time_sec,
               iperf_instance->stats.period_transfer,
               format_ch,
               iperf_instance->stats.period_bandwidth,
               format_ch);
        break;
    case IPERF_PRINT_SUMMARY:
        printf("[%3d]  0.0-%2" PRIu32 ".0 sec\t%2.2f %cBytes\t%.2f %cbits/sec\n", iperf_instance->id,
               iperf_instance->stats.current_time_sec,
               iperf_instance->stats.curr_total_transfer,
               format_ch,
               iperf_instance->stats.average_bandwidth.val,
               format_ch);
        break;
    // fallthrough
    default:
        break;
    }
}

static void iperf_state_action(iperf_state_t iperf_state, iperf_instance_data_t *iperf_instance)
{
    if (iperf_instance->report_handler) {
        iperf_instance->report_handler(iperf_instance->id, iperf_state, iperf_instance->report_handler_priv);
    }
    // release report task to print connect info
    if (iperf_state == IPERF_STARTED && iperf_instance->report_task_hdl) {
        xTaskNotifyGive(iperf_instance->report_task_hdl);
    }
}

IRAM_ATTR static void tx_timer_cb(void *arg)
{
    iperf_instance_data_t *iperf_instance = (iperf_instance_data_t *)arg;
    // notify traffic task that it's time to transmit
    xTaskNotifyGive(iperf_instance->traffic_task_hdl);
}

IRAM_ATTR static void tick_timer_cb(void *arg)
{
    iperf_instance_data_t *iperf_instance = (iperf_instance_data_t *)arg;

    iperf_instance->timers.ticks++;
    iperf_instance->timers.to_report_ticks++;

    if (iperf_instance->timers.to_report_ticks >= iperf_instance->interval || iperf_instance->timers.ticks >= iperf_instance->time) {
        iperf_report_task_t report_task_data_tmp = atomic_load(&iperf_instance->report_task_data);
        // zero indicates the report task processed data
        if (report_task_data_tmp.report_interval_sec == 0) {
            report_task_data_tmp.period_data_snapshot = atomic_exchange(&iperf_instance->period_data_passed, 0);
            report_task_data_tmp.report_interval_sec = iperf_instance->timers.to_report_ticks;
        } else {
            // if report task hasn't executed yet, try extend the report period
            report_task_data_tmp.period_data_snapshot += atomic_load(&iperf_instance->period_data_passed);
            report_task_data_tmp.report_interval_sec += iperf_instance->timers.to_report_ticks;
            ESP_LOGW(TAG_ID, "report_task is starving for execution time!");
        }
        atomic_store(&iperf_instance->report_task_data, report_task_data_tmp);
        iperf_instance->timers.to_report_ticks = 0;

        // iperf execution time elapsed
        if (iperf_instance->timers.ticks >= iperf_instance->time) {
            iperf_stop_exec(iperf_instance);
        } else {
            // notify report task that it's time to report
            xTaskNotifyGive(iperf_instance->report_task_hdl);
        }
    }
}

static esp_err_t iperf_create_timers(iperf_instance_data_t *iperf_instance)
{
    // setup timers
    esp_timer_create_args_t tick_timer_args = {
        .callback = &tick_timer_cb,
        .arg = iperf_instance,
        .name = "iperf_tick_timer"
    };
    ESP_RETURN_ON_ERROR(esp_timer_create(&tick_timer_args, &(iperf_instance->timers.tick_timer)), TAG_ID, "failed to create tick timer");

    if (iperf_instance->timers.tx_period_us > 0) {
        esp_timer_create_args_t tx_timer_args = {
            .callback = &tx_timer_cb,
            .arg = iperf_instance,
            .name = "iperf_tx_timer"
        };
        ESP_RETURN_ON_ERROR(esp_timer_create(&tx_timer_args, &(iperf_instance->timers.tx_timer)), TAG_ID, "failed to create Tx timer");
    }

    return ESP_OK;
}

static esp_err_t iperf_start_timers(iperf_instance_data_t *iperf_instance)
{
    if (iperf_instance->timers.tx_timer) {
        ESP_RETURN_ON_ERROR(esp_timer_start_periodic(iperf_instance->timers.tx_timer, iperf_instance->timers.tx_period_us),
                            TAG_ID, "failed to start Tx timer");
    }
    ESP_RETURN_ON_ERROR(esp_timer_start_periodic(iperf_instance->timers.tick_timer, IPERF_TICK_US),
                        TAG_ID, "failed to start tick timer");
    return ESP_OK;
}

static esp_err_t iperf_stop_timers(iperf_instance_data_t *iperf_instance)
{
    esp_err_t ret;
    if (iperf_instance->timers.tx_timer) {
        ret = esp_timer_stop(iperf_instance->timers.tx_timer);
    }
    ret = esp_timer_stop(iperf_instance->timers.tick_timer);
    return ret;
}

static esp_err_t iperf_delete_timers(iperf_instance_data_t *iperf_instance)
{
    esp_err_t ret;
    if (iperf_instance->timers.tx_timer) {
        ret = esp_timer_delete(iperf_instance->timers.tx_timer);
    }
    ret = esp_timer_delete(iperf_instance->timers.tick_timer);
    return ret;
}

static int iperf_show_socket_error_reason(iperf_instance_data_t *iperf_instance, const char *str)
{
    int err = errno;
    if (err != 0) {
        ESP_LOGW(TAG_ID, "%s error, error code: %d, reason: %s", str, err, strerror(err));
    }

    return err;
}

inline static bool iperf_is_udp_client(iperf_instance_data_t *iperf_instance)
{
    return ((iperf_instance->flags & IPERF_FLAG_CLIENT) && (iperf_instance->flags & IPERF_FLAG_UDP));
}

inline static bool iperf_is_udp_server(iperf_instance_data_t *iperf_instance)
{
    return ((iperf_instance->flags & IPERF_FLAG_SERVER) && (iperf_instance->flags & IPERF_FLAG_UDP));
}

inline static bool iperf_is_tcp_client(iperf_instance_data_t *iperf_instance)
{
    return ((iperf_instance->flags & IPERF_FLAG_CLIENT) && (iperf_instance->flags & IPERF_FLAG_TCP));
}

inline static bool iperf_is_tcp_server(iperf_instance_data_t *iperf_instance)
{
    return ((iperf_instance->flags & IPERF_FLAG_SERVER) && (iperf_instance->flags & IPERF_FLAG_TCP));
}

static void iperf_report_task(void *arg)
{
    iperf_instance_data_t *iperf_instance = (iperf_instance_data_t *) arg;
    double data_len;
    bool connnect_info_printed = false;

    do {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (!connnect_info_printed) {
            iperf_print_report(IPERF_PRINT_CONNECT_INFO, iperf_instance);
            connnect_info_printed = true;
        }
        iperf_report_task_t report_task_data_tmp = { 0 };
        // load a local copy and zero the atomic var to indicate report has been processed
        report_task_data_tmp = atomic_exchange(&iperf_instance->report_task_data, report_task_data_tmp);

        // check if valid data is available
        if (report_task_data_tmp.report_interval_sec != 0) {
            switch (iperf_instance->stats.format) {
            case KBITS_PER_SEC:
                data_len = (report_task_data_tmp.period_data_snapshot / 1000.0);
                break;
            case MBITS_PER_SEC:
            /* pass through */
            default:
                data_len = (report_task_data_tmp.period_data_snapshot / 1000.0 / 1000.0);
                break;
            }
            iperf_instance->stats.period_bandwidth = data_len * 8 / report_task_data_tmp.report_interval_sec;
            iperf_instance->stats.average_bandwidth.k++;
            iperf_instance->stats.average_bandwidth.val = ((iperf_instance->stats.average_bandwidth.val * (iperf_instance->stats.average_bandwidth.k - 1) /
                                                            iperf_instance->stats.average_bandwidth.k) +
                                                           (iperf_instance->stats.period_bandwidth / iperf_instance->stats.average_bandwidth.k));
            iperf_instance->stats.period_transfer = data_len;
            iperf_instance->stats.curr_total_transfer += data_len;
            iperf_instance->stats.prev_time_sec = iperf_instance->stats.current_time_sec;
            iperf_instance->stats.current_time_sec += report_task_data_tmp.report_interval_sec;

            // report IPERF_RUNNING to the handler
            iperf_state_action(IPERF_RUNNING, iperf_instance);
            iperf_print_report(IPERF_PRINT_STATS, iperf_instance);
        }
    } while (iperf_instance->is_running);

    if (iperf_instance->stats.current_time_sec != 0) {
        iperf_print_report(IPERF_PRINT_SUMMARY, iperf_instance);
    }

    iperf_instance->report_task_hdl = NULL;
    ESP_LOGD(TAG_ID, "report task to be deleted");
    vTaskDelete(NULL);
}

IRAM_ATTR static esp_err_t iperf_client_loop(iperf_instance_data_t *iperf_instance)
{
    esp_err_t ret = ESP_OK;
    socklen_t addr_len = sizeof(struct sockaddr);
    uint32_t pkt_cnt = 0;
    const char* error_log = "sendto";
    int want_send = iperf_instance->socket_info.buffer_len;
    uint32_t* pkt_id_p = (uint32_t *) iperf_instance->socket_info.buffer;
    bool is_started = false;

    // we don't clear count on exit so if Tx was delayed by printing report, we execute with shorter period next time and so
    // can catch up the delay
    while (iperf_instance->timers.tx_timer == NULL || ulTaskNotifyTake(pdFALSE, portMAX_DELAY) > 0) {
        if (!iperf_instance->is_running) {
            break;
        }
        *pkt_id_p = htonl(pkt_cnt++); // datagrams need to be sequentially numbered
        int actual_send = sendto(iperf_instance->socket, iperf_instance->socket_info.buffer, want_send,
                                 0, (struct sockaddr *) &iperf_instance->socket_info.target_addr, addr_len);
        if (actual_send != want_send) {
            if (iperf_instance->flags & IPERF_FLAG_UDP) {
                // ENOMEM & ENOBUFS is expected under heavy load => do not print it
                if ((errno != ENOMEM) && (errno != ENOBUFS)) {
                    iperf_show_socket_error_reason(iperf_instance, error_log);
                    ret = ESP_FAIL;
                    goto err;
                }
            } else if (iperf_instance->flags & IPERF_FLAG_TCP) {
                iperf_show_socket_error_reason(iperf_instance, error_log);
                ret = ESP_FAIL;
                goto err;
            }
        } else {
            atomic_fetch_add(&(iperf_instance->period_data_passed), actual_send);
            if (!is_started) {
                iperf_state_action(IPERF_STARTED, iperf_instance);
                is_started = true;
            }
        }
    };
err:
    if (is_started) {
        iperf_state_action(IPERF_STOPPED, iperf_instance);
    }
    return ret;
}

IRAM_ATTR static esp_err_t iperf_server_loop(iperf_instance_data_t *iperf_instance)
{
    esp_err_t ret = ESP_OK;
    int want_recv = iperf_instance->socket_info.buffer_len;
    int actual_recv = 0;
    bool is_started = false;

#if IPERF_IPV6_ENABLED && IPERF_IPV4_ENABLED
    socklen_t socklen = (iperf_instance->socket_info.source.type == ESP_IPADDR_TYPE_V6) ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in);
#elif IPERF_IPV6_ENABLED
    socklen_t socklen = sizeof(struct sockaddr_in6);
#else
    socklen_t socklen = sizeof(struct sockaddr_in);
#endif
    const char *error_log = "recvfrom";

    while (iperf_instance->is_running) {
        actual_recv = recvfrom(iperf_instance->socket, iperf_instance->socket_info.buffer, want_recv, 0,
                               (struct sockaddr *)&iperf_instance->socket_info.target_addr, &socklen);

        if (actual_recv == -1) {
            iperf_show_socket_error_reason(iperf_instance, error_log);
            ret = ESP_FAIL;
            goto err;
        }
        atomic_fetch_add(&(iperf_instance->period_data_passed), actual_recv);
        if (!is_started) {
            ESP_RETURN_ON_ERROR(iperf_start_timers(iperf_instance), TAG_ID, "failed to start internal timers");
            iperf_state_action(IPERF_STARTED, iperf_instance);
            is_started = true;
        }
    }
err:
    if (is_started) {
        iperf_state_action(IPERF_STOPPED, iperf_instance);
    }
    return ret;
}

static esp_err_t iperf_start_and_run_tcp_server(iperf_instance_data_t *iperf_instance)
{
    int listen_socket = -1;
    int opt = 1;
    esp_err_t ret = ESP_OK;
    struct timeval timeout = { 0 };
    socklen_t addr_len = sizeof(struct sockaddr);
    struct sockaddr_storage listen_addr = { 0 };
#if IPERF_IPV4_ENABLED
    struct sockaddr_in listen_addr4 = { 0 };
    struct sockaddr_in remote_addr = { 0 };
#endif
#if IPERF_IPV6_ENABLED
    struct sockaddr_in6 listen_addr6 = { 0 };
    struct sockaddr_in6 remote_addr6 = { 0 };
#endif
    if (iperf_instance->socket_info.source.type == ESP_IPADDR_TYPE_V6) {
#if IPERF_IPV6_ENABLED
        memcpy(&listen_addr6.sin6_addr, iperf_instance->socket_info.source.u_addr.ip6.addr, sizeof(listen_addr6.sin6_addr));
        listen_addr6.sin6_family = AF_INET6;
        listen_addr6.sin6_port = htons(iperf_instance->socket_info.sport);

        listen_socket = socket(AF_INET6, SOCK_STREAM, IPPROTO_IPV6);
        ESP_GOTO_ON_FALSE((listen_socket >= 0), ESP_FAIL, err, TAG_ID, "cannot start TCP server: unable to create socket - errno %d", errno);

        ESP_GOTO_ON_FALSE(setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == 0, ESP_FAIL, err, TAG_ID, "failed to set SO_REUSEADDR - errno %d", errno);
        ESP_GOTO_ON_FALSE(setsockopt(listen_socket, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt)) == 0, ESP_FAIL, err, TAG_ID, "failed to set IPV6_V6ONLY - errno %d", errno);

        ESP_LOGD(TAG_ID, "Socket created");

        ESP_GOTO_ON_FALSE(bind(listen_socket, (struct sockaddr *)&listen_addr6, sizeof(listen_addr6)) == 0,
                          ESP_FAIL, err, TAG_ID, "cannot start TCP server: socket is unable to bind - errno %d, IPPROTO: %d", errno, AF_INET6);
        ESP_GOTO_ON_FALSE(listen(listen_socket, 0) == 0, ESP_FAIL, err, TAG_ID, "cannot start TCP server: an error occurred during listen - errno %d", errno);

        memcpy(&listen_addr, &listen_addr6, sizeof(listen_addr6));
#else
        ESP_GOTO_ON_FALSE(false, ESP_ERR_INVALID_ARG, err, TAG_ID, "cannot start TCP server: invalid address type");
#endif
    } else if (iperf_instance->socket_info.source.type == ESP_IPADDR_TYPE_V4) {
#if IPERF_IPV4_ENABLED
        listen_addr4.sin_family = AF_INET;
        listen_addr4.sin_port = htons(iperf_instance->socket_info.sport);
        listen_addr4.sin_addr.s_addr = iperf_instance->socket_info.source.u_addr.ip4.addr;

        listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        ESP_GOTO_ON_FALSE((listen_socket >= 0), ESP_FAIL, err, TAG_ID, "cannot start TCP server: unable to create socket - errno %d", errno);

        ESP_GOTO_ON_FALSE(setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == 0, ESP_FAIL, err, TAG_ID, "failed to set SO_REUSEADDR - errno %d", errno);

        ESP_LOGD(TAG_ID, "Socket created");

        ESP_GOTO_ON_FALSE(bind(listen_socket, (struct sockaddr *)&listen_addr4, sizeof(listen_addr4)) == 0,
                          ESP_FAIL, err, TAG_ID, "cannot start TCP server: socket is unable to bind - errno %d, IPPROTO: %d", errno, AF_INET);

        ESP_GOTO_ON_FALSE(listen(listen_socket, 0) == 0, ESP_FAIL, err, TAG_ID, "cannot start TCP server: an error occurred during listen - errno %d", errno);
        memcpy(&listen_addr, &listen_addr4, sizeof(listen_addr4));
#else
        ESP_GOTO_ON_FALSE(false, ESP_ERR_INVALID_ARG, err, TAG_ID, "cannot start TCP server: invalid address type");
#endif
    }

    timeout.tv_sec = IPERF_SOCKET_ACCEPT_TIMEOUT;
    ESP_GOTO_ON_FALSE(setsockopt(listen_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0, ESP_FAIL, err, TAG_ID, "failed to set SO_RCVTIMEO - errno %d", errno);

    if (iperf_instance->socket_info.source.type == ESP_IPADDR_TYPE_V6) {
#if IPERF_IPV6_ENABLED
        iperf_instance->socket = accept(listen_socket, (struct sockaddr *)&remote_addr6, &addr_len);
        ESP_GOTO_ON_FALSE((iperf_instance->socket >= 0), ESP_FAIL, err, TAG_ID, "cannot start TCP server: socket is unable to accept connection - errno %d", errno);
        char addr_str[INET6_ADDRSTRLEN];
        inet6_ntoa_r(remote_addr6.sin6_addr, addr_str, sizeof(addr_str));
        ESP_LOGD(TAG_ID, "accept: [%s]:%d", addr_str, htons(remote_addr6.sin6_port));
#endif
    } else if (iperf_instance->socket_info.source.type == ESP_IPADDR_TYPE_V4) {
#if IPERF_IPV4_ENABLED
        iperf_instance->socket = accept(listen_socket, (struct sockaddr *)&remote_addr, &addr_len);
        ESP_GOTO_ON_FALSE((iperf_instance->socket >= 0), ESP_FAIL, err, TAG_ID, "cannot start TCP server: socket is unable to accept connection - errno %d, socket: %d", errno, listen_socket);
        char addr_str[INET_ADDRSTRLEN];
        inet_ntoa_r(remote_addr.sin_addr, addr_str, sizeof(addr_str));
        ESP_LOGD(TAG_ID, "accept: %s:%d", addr_str, htons(remote_addr.sin_port));
#endif
    }

    timeout.tv_sec = IPERF_SOCKET_RX_TIMEOUT;
    ESP_GOTO_ON_FALSE(setsockopt(iperf_instance->socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0, ESP_FAIL, err, TAG_ID, "failed to set SO_RCVTIMEO - errno %d", errno);
    iperf_instance->socket_info.target_addr = listen_addr;

    // if loop finished prematurely due to error
    if (iperf_server_loop(iperf_instance) != ESP_OK) {
        goto err;
    }
    goto exit;
err:
    iperf_stop_exec(iperf_instance);
exit:
    if (iperf_instance->socket != -1) {
        shutdown(iperf_instance->socket, 0);
        close(iperf_instance->socket);
        ESP_LOGD(TAG_ID, "TCP Socket server is closed.");
    }

    if (listen_socket != -1) {
        shutdown(listen_socket, 0);
        close(listen_socket);
        ESP_LOGD(TAG_ID, "TCP listen socket is closed.");
    }
    return ret;
}

static esp_err_t iperf_start_and_run_tcp_client(iperf_instance_data_t *iperf_instance)
{
    esp_err_t ret = ESP_OK;
    struct timeval timeout = { 0 };
#if IPERF_IPV4_ENABLED
    struct sockaddr_in dest_addr4 = { 0 };
#endif
#if IPERF_IPV6_ENABLED
    struct sockaddr_in6 dest_addr6 = { 0 };
#endif
    if (iperf_instance->socket_info.destination.type == ESP_IPADDR_TYPE_V6) {
#if IPERF_IPV6_ENABLED
        iperf_instance->socket = socket(AF_INET6, SOCK_STREAM, IPPROTO_IPV6);
        ESP_GOTO_ON_FALSE((iperf_instance->socket >= 0), ESP_FAIL, err, TAG_ID, "cannot start TCP client: unable to create socket - errno %d", errno);

        memcpy(&dest_addr6.sin6_addr, iperf_instance->socket_info.destination.u_addr.ip6.addr, 16); // ipv6 address is 16 bytes
        dest_addr6.sin6_family = AF_INET6;
        dest_addr6.sin6_port = htons(iperf_instance->socket_info.dport);

        ESP_GOTO_ON_FALSE(connect(iperf_instance->socket, (struct sockaddr *)&dest_addr6, sizeof(struct sockaddr_in6)) == 0,
                          ESP_FAIL, err, TAG_ID, "cannot start TCP client: socket is unable to connect - errno %d", errno);
        ESP_LOGD(TAG_ID, "Successfully connected");
        memcpy(&iperf_instance->socket_info.target_addr, &dest_addr6, sizeof(dest_addr6));
#else
        ESP_GOTO_ON_FALSE(false, ESP_ERR_INVALID_ARG, err, TAG_ID, "cannot start TCP client: invalid address type");
#endif
    } else if (iperf_instance->socket_info.destination.type == ESP_IPADDR_TYPE_V4) {
#if IPERF_IPV4_ENABLED
        iperf_instance->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        ESP_GOTO_ON_FALSE((iperf_instance->socket >= 0), ESP_FAIL, err, TAG_ID, "cannot start TCP client: unable to create socket - errno %d", errno);

        dest_addr4.sin_family = AF_INET;
        dest_addr4.sin_port = htons(iperf_instance->socket_info.dport);
        dest_addr4.sin_addr.s_addr = iperf_instance->socket_info.destination.u_addr.ip4.addr;
        ESP_GOTO_ON_FALSE(connect(iperf_instance->socket, (struct sockaddr *)&dest_addr4, sizeof(struct sockaddr_in)) == 0,
                          ESP_FAIL, err, TAG_ID, "cannot start TCP client: socket is unable to connect - errno %d", errno);
        ESP_LOGD(TAG_ID, "Successfully connected");
        memcpy(&iperf_instance->socket_info.target_addr, &dest_addr4, sizeof(dest_addr4));
#else
        ESP_GOTO_ON_FALSE(false, ESP_ERR_INVALID_ARG, err, TAG_ID, "cannot start TCP client: invalid address type");
#endif
    }
    timeout.tv_sec = IPERF_SOCKET_TCP_TX_TIMEOUT;
    ESP_GOTO_ON_FALSE(setsockopt(iperf_instance->socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == 0, ESP_FAIL, err, TAG_ID, "failed to set IPERF_SOCKET_TCP_TX_TIMEOUT - errno %d", errno);

    ESP_GOTO_ON_ERROR(iperf_start_timers(iperf_instance), err, TAG_ID, "failed to start internal timers");
    // if loop finished prematurely due to error
    if (iperf_client_loop(iperf_instance) != ESP_OK) {
        goto err;
    }
    goto exit;
err:
    iperf_stop_exec(iperf_instance);
exit:
    if (iperf_instance->socket != -1) {
        shutdown(iperf_instance->socket, 0);
        close(iperf_instance->socket);
        iperf_instance->socket = -1;
        ESP_LOGD(TAG_ID, "TCP Socket client is closed.");
    }
    return ret;
}

static esp_err_t iperf_start_and_run_udp_server(iperf_instance_data_t *iperf_instance)
{
    int opt = 1;
    esp_err_t ret = ESP_OK;
    struct timeval timeout = { 0 };
    struct sockaddr_storage listen_addr = { 0 };
#if IPERF_IPV4_ENABLED
    struct sockaddr_in listen_addr4 = { 0 };
#endif
#if IPERF_IPV6_ENABLED
    struct sockaddr_in6 listen_addr6 = { 0 };
#endif
    if (iperf_instance->socket_info.source.type == ESP_IPADDR_TYPE_V6) {
#if IPERF_IPV6_ENABLED
        memcpy(&listen_addr6.sin6_addr, iperf_instance->socket_info.source.u_addr.ip6.addr, sizeof(listen_addr6.sin6_addr));
        listen_addr6.sin6_family = AF_INET6;
        listen_addr6.sin6_port = htons(iperf_instance->socket_info.sport);

        iperf_instance->socket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
        ESP_GOTO_ON_FALSE((iperf_instance->socket >= 0), ESP_FAIL, err, TAG_ID, "cannot start UDP server: unable to create socket - errno %d", errno);
        ESP_LOGD(TAG_ID, "UDP socket created");

        ESP_GOTO_ON_FALSE(setsockopt(iperf_instance->socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == 0, ESP_FAIL, err, TAG_ID, "failed to set SO_REUSEADDR - errno %d", errno);

        ESP_GOTO_ON_FALSE(bind(iperf_instance->socket, (struct sockaddr *)&listen_addr6, sizeof(struct sockaddr_in6)) == 0,
                          ESP_FAIL, err, TAG_ID, "cannot start UDP server: socket is unable to bind - errno %d", errno);
        char addr_str[INET6_ADDRSTRLEN];
        inet6_ntoa_r(listen_addr6.sin6_addr, addr_str, sizeof(addr_str));
        ESP_LOGD(TAG_ID, "UDP server socket bound, [%s]:%" PRIu16, addr_str, ntohs(listen_addr6.sin6_port));

        memcpy(&listen_addr, &listen_addr6, sizeof(listen_addr6));
#else
        ESP_GOTO_ON_FALSE(false, ESP_ERR_INVALID_ARG, err, TAG_ID, "cannot start UDP server: invalid address type");
#endif
    } else if (iperf_instance->socket_info.source.type == ESP_IPADDR_TYPE_V4) {
#if IPERF_IPV4_ENABLED
        listen_addr4.sin_family = AF_INET;
        listen_addr4.sin_port = htons(iperf_instance->socket_info.sport);
        listen_addr4.sin_addr.s_addr = iperf_instance->socket_info.source.u_addr.ip4.addr;
        iperf_instance->socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        ESP_GOTO_ON_FALSE((iperf_instance->socket >= 0), ESP_FAIL, err, TAG_ID, "cannot start UDP server: unable to create socket - errno %d", errno);
        ESP_LOGD(TAG_ID, "UDP socket created");

        ESP_GOTO_ON_FALSE(setsockopt(iperf_instance->socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == 0, ESP_FAIL, err, TAG_ID, "failed to set SO_REUSEADDR - errno %d", errno);
        ESP_GOTO_ON_FALSE(bind(iperf_instance->socket, (struct sockaddr *)&listen_addr4, sizeof(struct sockaddr_in)) == 0,
                          ESP_FAIL, err, TAG_ID, "cannot start UDP server: socket is unable to bind - errno %d", errno);
        char addr_str[INET_ADDRSTRLEN];
        inet_ntoa_r(listen_addr4.sin_addr, addr_str, sizeof(addr_str));
        ESP_LOGD(TAG_ID, "UDP server socket bound, %s:%" PRIu16, addr_str, ntohs(listen_addr4.sin_port));
        memcpy(&listen_addr, &listen_addr4, sizeof(listen_addr4));
#else
        ESP_GOTO_ON_FALSE(false, ESP_ERR_INVALID_ARG, err, TAG_ID, "cannot start UDP server: invalid address type");
#endif
    }

    timeout.tv_sec = IPERF_SOCKET_RX_TIMEOUT;
    ESP_GOTO_ON_FALSE(setsockopt(iperf_instance->socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0, ESP_FAIL, err, TAG_ID, "failed to set SO_RCVTIMEO - errno %d", errno);
    iperf_instance->socket_info.target_addr = listen_addr;

    // if loop finished prematurely due to error
    if (iperf_server_loop(iperf_instance) != ESP_OK) {
        goto err;
    }
    goto exit;
err:
    iperf_stop_exec(iperf_instance);
exit:
    if (iperf_instance->socket != -1) {
        shutdown(iperf_instance->socket, 0);
        close(iperf_instance->socket);
        iperf_instance->socket = -1;
    }
    ESP_LOGD(TAG_ID, "UDP socket server is closed.");
    return ret;
}

static esp_err_t iperf_start_and_run_udp_client(iperf_instance_data_t *iperf_instance)
{
    int opt = 1;
    esp_err_t ret = ESP_OK;
#if IPERF_IPV4_ENABLED
    struct sockaddr_in dest_addr4 = { 0 };
#endif
#if IPERF_IPV6_ENABLED
    struct sockaddr_in6 dest_addr6 = { 0 };
#endif

    if (iperf_instance->socket_info.destination.type == ESP_IPADDR_TYPE_V6) {
#if IPERF_IPV6_ENABLED
        memcpy(&dest_addr6.sin6_addr, iperf_instance->socket_info.destination.u_addr.ip6.addr, 16); // ipv6 address is 16 bytes
        dest_addr6.sin6_family = AF_INET6;
        dest_addr6.sin6_port = htons(iperf_instance->socket_info.dport);

        iperf_instance->socket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_IPV6);
        ESP_GOTO_ON_FALSE((iperf_instance->socket >= 0), ESP_FAIL, err, TAG_ID, "cannot start UDP client: unable to create socket - errno %d", errno);
        ESP_LOGD(TAG_ID, "UDP socket created, sending to [" IPV6STR "]:%d", IPV62STR(iperf_instance->socket_info.destination.u_addr.ip6), iperf_instance->socket_info.dport);

        ESP_GOTO_ON_FALSE(setsockopt(iperf_instance->socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == 0, ESP_FAIL, err, TAG_ID, "failed to set SO_REUSEADDR - errno %d", errno);
        memcpy(&iperf_instance->socket_info.target_addr, &dest_addr6, sizeof(dest_addr6));

        if (!ip6_addr_isany(&iperf_instance->socket_info.source.u_addr.ip6) || iperf_instance->socket_info.sport) {
            struct sockaddr_in6 src_addr6;
            src_addr6.sin6_family = AF_INET6;
            src_addr6.sin6_port = htons(iperf_instance->socket_info.sport);
            memcpy(&src_addr6.sin6_addr, iperf_instance->socket_info.source.u_addr.ip6.addr, 16);

            ESP_GOTO_ON_FALSE(bind(iperf_instance->socket, (struct sockaddr *)&src_addr6, sizeof(src_addr6)) == 0,
                              ESP_FAIL, err, TAG_ID, "cannot start UDP client: unable to bind socket - errno %d", errno);
            char addr_str[INET6_ADDRSTRLEN];
            inet6_ntoa_r(src_addr6.sin6_addr, addr_str, sizeof(addr_str));
            ESP_LOGD(TAG_ID, "UDP client socket bound [%s]:%d", addr_str, iperf_instance->socket_info.sport);
        }
#else
        ESP_GOTO_ON_FALSE(false, ESP_ERR_INVALID_ARG, err, TAG_ID, "cannot start UDP client: invalid address type");
#endif
    } else if (iperf_instance->socket_info.destination.type == ESP_IPADDR_TYPE_V4) {
#if IPERF_IPV4_ENABLED
        dest_addr4.sin_family = AF_INET;
        dest_addr4.sin_port = htons(iperf_instance->socket_info.dport);
        dest_addr4.sin_addr.s_addr = iperf_instance->socket_info.destination.u_addr.ip4.addr;

        iperf_instance->socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        ESP_GOTO_ON_FALSE((iperf_instance->socket >= 0), ESP_FAIL, err, TAG_ID, "cannot start UDP client: unable to create socket - errno %d", errno);
        ESP_LOGD(TAG_ID, "UDP socket created, sending to " IPSTR ":%d", IP2STR(&iperf_instance->socket_info.destination.u_addr.ip4), iperf_instance->socket_info.dport);

        ESP_GOTO_ON_FALSE(setsockopt(iperf_instance->socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == 0, ESP_FAIL, err, TAG_ID, "failed to set SO_REUSEADDR - errno %d", errno);
        memcpy(&iperf_instance->socket_info.target_addr, &dest_addr4, sizeof(dest_addr4));

        if (!ip4_addr_isany(&iperf_instance->socket_info.source.u_addr.ip4) || iperf_instance->socket_info.sport) {
            struct sockaddr_in src_addr;
            src_addr.sin_family = AF_INET;
            src_addr.sin_port = htons(iperf_instance->socket_info.sport);
            src_addr.sin_addr.s_addr = iperf_instance->socket_info.source.u_addr.ip4.addr;

            ESP_GOTO_ON_FALSE(bind(iperf_instance->socket, (struct sockaddr *)&src_addr, sizeof(src_addr)) == 0,
                              ESP_FAIL, err, TAG_ID, "cannot start UDP client: unable to bind socket - errno %d", errno);
            char addr_str[INET_ADDRSTRLEN];
            inet_ntoa_r(src_addr.sin_addr, addr_str, sizeof(addr_str));
            ESP_LOGD(TAG_ID, "UDP client socket bound, %s:%d", addr_str, ntohs(src_addr.sin_port));
        }
#else
        ESP_GOTO_ON_FALSE(false, ESP_ERR_INVALID_ARG, err, TAG_ID, "cannot start UDP client: invalid address type");
#endif
    }

    ESP_GOTO_ON_ERROR(iperf_start_timers(iperf_instance), err, TAG_ID, "failed to start internal timers");
    // if loop finished prematurely due to error
    if (iperf_client_loop(iperf_instance) != ESP_OK) {
        goto err;
    }
    goto exit;
err:
    iperf_stop_exec(iperf_instance);
exit:
    if (iperf_instance->socket != -1) {
        shutdown(iperf_instance->socket, 0);
        close(iperf_instance->socket);
        iperf_instance->socket = -1;
    }
    ESP_LOGD(TAG_ID, "UDP Socket client is closed");
    return ret;
}

static void iperf_traffic_task(void *arg)
{
    // prevent compiler from producing warning because ret is not used, but needed by ESP_GOTO_ON_ERROR
    __attribute__((unused)) esp_err_t ret;
    iperf_instance_data_t *iperf_instance = (iperf_instance_data_t *) arg;
    if (iperf_is_tcp_server(iperf_instance)) {
        ESP_GOTO_ON_ERROR(iperf_start_and_run_tcp_server(iperf_instance), err, TAG_ID, "an error occurred while running tcp server");
    } else if (iperf_is_tcp_client(iperf_instance)) {
        ESP_GOTO_ON_ERROR(iperf_start_and_run_tcp_client(iperf_instance), err, TAG_ID, "an error occurred while running tcp client");
    }  else if (iperf_is_udp_server(iperf_instance)) {
        ESP_GOTO_ON_ERROR(iperf_start_and_run_udp_server(iperf_instance), err, TAG_ID, "an error occurred while running udp server");
    }  else if (iperf_is_udp_client(iperf_instance)) {
        ESP_GOTO_ON_ERROR(iperf_start_and_run_udp_client(iperf_instance), err, TAG_ID, "an error occurred while running udp client");
    } else {
        ESP_GOTO_ON_FALSE(false, ESP_ERR_INVALID_ARG, err, TAG_ID, "cannot start iperf-related tasks: invalid mode (TCP/UDP) or role (client/server)");
    }
err:
    iperf_instance->traffic_task_hdl = NULL;
    iperf_delete_instance(iperf_instance);
    ESP_LOGD(TAG_ID, "traffic task is to be deleted");
    vTaskDelete(NULL);
}

static esp_err_t iperf_stop_exec(iperf_instance_data_t *iperf_instance)
{
    // stop timers
    iperf_stop_timers(iperf_instance);

    // prevent further exec of while loops
    iperf_instance->is_running = false;

    // release possibly blocking client loop
    if ((iperf_instance->flags & IPERF_FLAG_CLIENT) && iperf_instance->timers.tx_timer) {
        if (iperf_instance->traffic_task_hdl) {
            xTaskNotifyGive(iperf_instance->traffic_task_hdl);
        }
    }
    // release possibly blocking server recv loop
    if ((iperf_instance->flags & IPERF_FLAG_SERVER) && iperf_instance->socket != -1) {
        shutdown(iperf_instance->socket, 0);
        close(iperf_instance->socket);
        iperf_instance->socket = -1;
    }

    // release report task
    if (iperf_instance->report_task_hdl) {
        xTaskNotifyGive(iperf_instance->report_task_hdl);
    }

    return ESP_OK;
}

static esp_err_t iperf_start_tasks(iperf_instance_data_t *iperf_instance, const iperf_cfg_t *cfg)
{
    esp_err_t ret = ESP_OK;
    BaseType_t xResult = pdPASS;

    // Note: task creation order matters due to the error handling and instance clean up sequence
    // Start report task
    xResult = xTaskCreate(iperf_report_task, IPERF_REPORT_TASK_NAME, IPERF_REPORT_TASK_STACK, (void*)iperf_instance,
                          IPERF_REPORT_TASK_PRIORITY, &(iperf_instance->report_task_hdl));
    ESP_GOTO_ON_FALSE(xResult == pdPASS, ESP_FAIL, err_report, TAG_ID, "cannot start iperf-related tasks: could not start report task");

    // Set task priority to default if not specified
    uint8_t traffic_task_priority = IPERF_DEFAULT_TRAFFIC_TASK_PRIORITY;
    if (cfg->traffic_task_priority != 0) {
        traffic_task_priority = cfg->traffic_task_priority;
    }
    iperf_instance->is_running = true;
    xResult = xTaskCreatePinnedToCore(iperf_traffic_task, IPERF_TRAFFIC_TASK_NAME, IPERF_TRAFFIC_TASK_STACK,
                                      (void*)iperf_instance, traffic_task_priority, &(iperf_instance->traffic_task_hdl),
                                      NUMBER_OF_CORES - 1);
    ESP_GOTO_ON_FALSE(xResult == pdPASS, ESP_FAIL, err_traffic, TAG_ID, "cannot start iperf-related tasks: could not start traffic task");
    return ESP_OK;
err_traffic:
    iperf_stop_exec(iperf_instance);
err_report:
    return ret;
}

static void iperf_copy_stats_to_report(iperf_instance_data_t *iperf_instance, iperf_report_t *report)
{
    report->output_format = iperf_instance->stats.format;
    report->period_bandwidth = iperf_instance->stats.period_bandwidth;
    report->period_transfer = iperf_instance->stats.period_transfer;
    report->average_bandwidth = iperf_instance->stats.average_bandwidth.val;
    report->curr_total_transfer = iperf_instance->stats.curr_total_transfer;
    report->current_time_sec = iperf_instance->stats.current_time_sec;
}

static int iperf_esp_err_to_errno(esp_err_t esp_err)
{
    switch (esp_err) {
    case ESP_ERR_NO_MEM:
        return ENOBUFS;
    case ESP_ERR_INVALID_ARG:
        return EINVAL;
    case ESP_ERR_TIMEOUT:
        return EBUSY;
    case ESP_ERR_INVALID_STATE:
        return ENODEV;
    }
    return EIO;
}

static esp_err_t iperf_list_exec_for_each(iperf_list_exec_fn fn, void *ctx)
{
    esp_err_t ret = ESP_OK;
    if (xSemaphoreTake(s_list_lock, IPERF_LIST_LOCK_TMO_RTOS_TICKS) == pdTRUE) {
        iperf_instance_data_t *instance;
        LIST_FOREACH(instance, p_running_iperfs_list, _list_entry) {
            ret = fn(instance, ctx);
            if (ret != ESP_OK) {
                break;
            }
        }
        xSemaphoreGive(s_list_lock);
    } else {
        ret = ESP_ERR_TIMEOUT;
    }
    return ret;
}

static iperf_id_t iperf_list_get_next_available_id_unsafe(void)
{
    iperf_id_t smallest_id = -1;
    iperf_instance_data_t *instance;

    LIST_FOREACH(instance, p_running_iperfs_list, _list_entry) {
        if (instance->id > smallest_id) {
            smallest_id = instance->id;
        }
    }
    return smallest_id + 1;
}

static iperf_instance_data_t* iperf_list_get_instance_by_id(iperf_id_t id)
{
    iperf_instance_data_t *result = NULL;
    iperf_instance_data_t *instance;
    if (xSemaphoreTake(s_list_lock, IPERF_LIST_LOCK_TMO_RTOS_TICKS) == pdTRUE) {
        LIST_FOREACH(instance, p_running_iperfs_list, _list_entry) {
            if (instance->id == id) {
                result = instance;
                break;
            }
        }
        xSemaphoreGive(s_list_lock);
    }
    return result;
}

static esp_err_t iperf_list_add_instance(iperf_instance_data_t *iperf_instance)
{
    esp_err_t ret = ESP_OK;
    if (xSemaphoreTake(s_list_lock, IPERF_LIST_LOCK_TMO_RTOS_TICKS) == pdTRUE) {
        iperf_id_t id = iperf_list_get_next_available_id_unsafe();
        ESP_GOTO_ON_FALSE(id > -1, ESP_FAIL, err, TAG, "too many instances exist, can't assign an id");
        iperf_instance->id = id;

        LIST_INSERT_HEAD(p_running_iperfs_list, iperf_instance, _list_entry);
        xSemaphoreGive(s_list_lock);
    } else {
        ret = ESP_ERR_TIMEOUT;
    }
err:
    return ret;
}

static esp_err_t iperf_list_remove_instance(iperf_instance_data_t *iperf_instance)
{
    esp_err_t ret = ESP_OK;
    if (xSemaphoreTake(s_list_lock, IPERF_LIST_LOCK_TMO_RTOS_TICKS) == pdTRUE) {
        LIST_REMOVE(iperf_instance, _list_entry);
        xSemaphoreGive(s_list_lock);
    } else {
        ret = ESP_ERR_TIMEOUT;
    }
    return ret;
}

static uint32_t iperf_get_buffer_len(iperf_instance_data_t *iperf_instance, uint16_t data_len)
{
    if (iperf_is_udp_client(iperf_instance)) {
#if IPERF_IPV6_ENABLED
        if (data_len) {
            return data_len;
        } else if (iperf_instance->socket_info.destination.type == ESP_IPADDR_TYPE_V6) {
            return IPERF_DEFAULT_IPV6_UDP_TX_LEN;
        } else {
            return IPERF_DEFAULT_IPV4_UDP_TX_LEN;
        }
#else
        return (data_len == 0 ? IPERF_DEFAULT_IPV4_UDP_TX_LEN : data_len);
#endif
    } else if (iperf_is_udp_server(iperf_instance)) {
        return IPERF_DEFAULT_UDP_RX_LEN;
    } else if (iperf_is_tcp_client(iperf_instance)) {
        return (data_len == 0 ? IPERF_DEFAULT_TCP_TX_LEN : data_len);
    } else {
        return IPERF_DEFAULT_TCP_RX_LEN;
    }
    return 0;
}

static esp_err_t iperf_force_stop(iperf_instance_data_t *iperf_instance, void *ctx)
{
    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_FALSE(iperf_instance != NULL, ESP_FAIL, err, TAG, "cannot stop instance: instance not found");
    ESP_LOGI(TAG, "waiting for iperf instance id=%" PRIi8 " to stop...", iperf_instance->id);
    iperf_instance->time = IPERF_TIME_FORCE_ELAPSED;
err:
    return ret;
}

static void iperf_delete_instance(iperf_instance_data_t *iperf_instance)
{
    if (iperf_instance != NULL) {
        int32_t tmo = 0;
        // any blocking operation should have been be unblocked at this point, wait for tasks finish and clean their resources
        while ((iperf_instance->report_task_hdl != NULL || iperf_instance->traffic_task_hdl != NULL) &&
                tmo < IPERF_TASKS_FINISH_TMO_MS) {
            vTaskDelay(pdMS_TO_TICKS(10));
            tmo += 10;
        }
        if (tmo >= IPERF_TASKS_FINISH_TMO_MS) {
            ESP_LOGE(TAG_ID, "iperf task hanged, the instance cannot be cleared");
            // there is no safe corrective action so just return since it's safer to not clean the instance rather than
            // hanged task possibly accessing freed resource
            return;
        }

        ESP_LOGD(TAG_ID, "deleting iperf instance");
        iperf_delete_timers(iperf_instance);

        iperf_state_action(IPERF_CLOSED, iperf_instance);

        // remove from list
        iperf_list_remove_instance(iperf_instance);

        // free the allocated memory
        free(iperf_instance->socket_info.buffer);
        free(iperf_instance);
    }
}

esp_err_t iperf_get_traffic_type(iperf_id_t id, iperf_traffic_type_t *traffic_type)
{
    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_FALSE(id >= 0, ESP_ERR_INVALID_ARG, err, TAG, "invalid id provided (id=%" PRIi8 ")", id);
    ESP_GOTO_ON_FALSE(traffic_type != NULL, ESP_ERR_INVALID_ARG, err, TAG, "invalid pointer to traffic_type");
    iperf_instance_data_t *iperf_instance = iperf_list_get_instance_by_id(id);
    ESP_GOTO_ON_FALSE(iperf_instance != NULL, ESP_ERR_INVALID_STATE, err, TAG, "cannot get data: instance (id=%" PRIi8 ") was not found", id);
    if (iperf_is_tcp_server(iperf_instance)) {
        *traffic_type = IPERF_TCP_SERVER;
    } else if (iperf_is_tcp_client(iperf_instance)) {
        *traffic_type = IPERF_TCP_CLIENT;
    }  else if (iperf_is_udp_server(iperf_instance)) {
        *traffic_type = IPERF_UDP_SERVER;
    }  else if (iperf_is_udp_client(iperf_instance)) {
        *traffic_type = IPERF_UDP_CLIENT;
    } else {
        ESP_GOTO_ON_FALSE(false, ESP_ERR_INVALID_STATE, err, TAG_ID, "invalid configuration");
    }
err:
    return ret;
}

esp_err_t iperf_get_report(iperf_id_t id, iperf_report_t *report)
{
    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_FALSE(id >= 0, ESP_ERR_INVALID_ARG, err, TAG, "invalid id provided (id=%" PRIi8 ")", id);
    ESP_GOTO_ON_FALSE(report != NULL, ESP_ERR_INVALID_ARG, err, TAG, "invalid pointer to report");
    iperf_instance_data_t *iperf_instance = iperf_list_get_instance_by_id(id);
    ESP_GOTO_ON_FALSE(iperf_instance != NULL, ESP_ERR_INVALID_STATE, err, TAG, "cannot get data: instance (id=%" PRIi8 ") was not found", id);
    iperf_copy_stats_to_report(iperf_instance, report);
err:
    return ret;
}

esp_err_t iperf_register_report_handler(iperf_id_t id, iperf_report_handler_func_t handler, void *priv)
{
    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_FALSE(id >= 0, ESP_ERR_INVALID_ARG, err, TAG, "invalid id provided (id=%" PRIi8 ")", id);
    iperf_instance_data_t *iperf_instance = iperf_list_get_instance_by_id(id);
    ESP_GOTO_ON_FALSE(iperf_instance != NULL, ESP_ERR_INVALID_STATE, err, TAG, "cannot register handler: instance (id=%" PRIi8 ") was not found", id);
    iperf_instance->report_handler = handler;
    if (handler == NULL) {
        iperf_instance->report_handler_priv = NULL;
    } else {
        iperf_instance->report_handler_priv = priv;
    }
    return ESP_OK;
err:
    return ret;
}

iperf_id_t iperf_start_instance(const iperf_cfg_t *cfg)
{
    esp_err_t ret = ESP_OK;
    iperf_instance_data_t *iperf_instance = NULL;

    ESP_GOTO_ON_FALSE(cfg != NULL, ESP_ERR_INVALID_ARG, err, TAG, "cannot create iperf instance: no config provided");

    if (s_list_lock == NULL) {
        s_list_lock = xSemaphoreCreateMutexStatic(&s_list_lock_buffer);
        // Note that an error is unlikely when initializing mutex statically.
        // Also note that even if mutex was tried to be created multiple times due to racing condition during `if` evaluation,
        // create mutex API is atomic and the mutex handle returned is always the same when static mutex is initialized multiple times.
        ESP_GOTO_ON_FALSE(s_list_lock, ESP_FAIL, err, TAG, "failed to create static mutex");
    }

    // allocate iperf control structure
    iperf_instance = heap_caps_calloc(1, sizeof(iperf_instance_data_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    ESP_GOTO_ON_FALSE(iperf_instance, ESP_ERR_NO_MEM, err, TAG, "cannot create iperf instance: not enough memory");

    // Add pointer to the array of started iperfs
    ESP_GOTO_ON_ERROR(iperf_list_add_instance(iperf_instance), err, TAG, "cannot add new instance to instances list");

    // create instance specific tag used in logs
    snprintf(iperf_instance->tag, sizeof(iperf_instance->tag), TAG_ID_STR, iperf_instance->id);

    iperf_instance->stats.format = cfg->format;
    iperf_instance->time = cfg->time;
    iperf_instance->interval = cfg->interval;
    iperf_instance->flags = cfg->flag;
    if (cfg->report_handler != NULL) {
        iperf_instance->report_handler = cfg->report_handler;
        iperf_instance->report_handler_priv = cfg->report_handler_priv;
    }

    // populate config information about socket
    iperf_instance->socket_info.destination = cfg->destination;
    iperf_instance->socket_info.source = cfg->source;
    iperf_instance->socket_info.dport = cfg->dport;
    iperf_instance->socket_info.sport = cfg->sport;
    iperf_instance->socket_info.buffer_len = iperf_get_buffer_len(iperf_instance, cfg->len_send_buf);
    iperf_instance->socket_info.buffer = (uint8_t *) calloc(iperf_instance->socket_info.buffer_len, sizeof(uint8_t));
    ESP_GOTO_ON_FALSE(iperf_instance->socket_info.buffer, ESP_ERR_NO_MEM, err, TAG_ID, "cannot create iperf instance: not enough memory for buffer allocation");
    iperf_instance->socket = -1;

    // calculate timer period or set default
    if (cfg->bw_lim > 0) {
        iperf_instance->timers.tx_period_us = iperf_instance->socket_info.buffer_len * 8 / cfg->bw_lim;
    }
    ESP_GOTO_ON_ERROR(iperf_create_timers(iperf_instance), err, TAG_ID, "failed to create timers");

    // Start tasks associated with iperf
    ESP_GOTO_ON_ERROR(iperf_start_tasks(iperf_instance, cfg), err, TAG_ID, "cannot start iperf-related tasks: an error has occurred");

    // return the id assigned to this instance
    return iperf_instance->id;
err:
    iperf_delete_instance(iperf_instance);
    errno = iperf_esp_err_to_errno(ret);
    return -1;
}

esp_err_t iperf_stop_instance(iperf_id_t id)
{
    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_FALSE(id >= 0 || id == IPERF_ALL_INSTANCES_ID, ESP_ERR_INVALID_ARG, err, TAG, "cannot stop instance: invalid id provided (id=%" PRIi8 ")", id);

    // if user called stop prior start
    if (s_list_lock == NULL) {
        s_list_lock = xSemaphoreCreateMutexStatic(&s_list_lock_buffer);
        ESP_GOTO_ON_FALSE(s_list_lock, ESP_FAIL, err, TAG, "failed to create static mutex");
    }

    if (id == IPERF_ALL_INSTANCES_ID) {
        return iperf_list_exec_for_each(iperf_force_stop, NULL);
    }
    iperf_instance_data_t *iperf_instance = iperf_list_get_instance_by_id(id);
    return iperf_force_stop(iperf_instance, NULL);
err:
    return ret;
}
