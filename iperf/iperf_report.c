/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <inttypes.h>
#include <stdio.h>
#include "esp_log.h"
#include "iperf.h"
#include "iperf_private.h"

static const char *TAG = "iperf_report";


inline static void iperf_copy_report(iperf_report_type_t report_type, iperf_instance_data_t *iperf_instance, iperf_report_t *report)
{
    report->instance_id = iperf_instance->id;
    report->report_type = report_type;
    if (report_type == IPERF_REPORT_CONNECT_INFO) {
        report->connect_info.socket = iperf_instance->socket;
        report->connect_info.target_addr = iperf_instance->socket_info.target_addr;
    } else {
        memcpy(&(report->traffic), &(iperf_instance->traffic), sizeof(iperf_traffic_report_t));
    }
}

static void iperf_print_connect_info(const iperf_report_t *report)
{
    iperf_id_t instance_id = report->instance_id;
    const iperf_connect_info_report_t* connect_info = &(report->connect_info);
    if (connect_info->target_addr.ss_family == AF_INET) {
#if IPERF_IPV4_ENABLED
        struct sockaddr_in local_addr4 = { 0 };
        socklen_t addr4_len = sizeof(local_addr4);
        if (getsockname(connect_info->socket, (struct sockaddr *)&local_addr4, &addr4_len) == 0) {
            char localaddr_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &local_addr4.sin_addr.s_addr, localaddr_str, sizeof(localaddr_str));
            char targetaddr_str[INET_ADDRSTRLEN];
            struct sockaddr_in *target_addr4 = (struct sockaddr_in *)&connect_info->target_addr;
            inet_ntop(AF_INET, &target_addr4->sin_addr.s_addr, targetaddr_str, sizeof(targetaddr_str));
            printf("[%3d] local %s:%" PRIu16 " connected to %s:%" PRIu16 "\n",
                    instance_id,
                    localaddr_str, ntohs(local_addr4.sin_port),
                    targetaddr_str, ntohs(target_addr4->sin_port));
        }
#endif
    } else if (connect_info->target_addr.ss_family == AF_INET6) {
#if IPERF_IPV6_ENABLED
        struct sockaddr_in6 local_addr6 = { 0 };
        socklen_t addr6_len = sizeof(local_addr6);
        if (getsockname(connect_info->socket, (struct sockaddr *)&local_addr6, &addr6_len) == 0) {
            char localaddr_str[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, &local_addr6.sin6_addr, localaddr_str, sizeof(localaddr_str));
            char targetaddr_str[INET6_ADDRSTRLEN];
            struct sockaddr_in6 *target_addr6 = (struct sockaddr_in6 *)&connect_info->target_addr;
            inet_ntop(AF_INET6, &target_addr6->sin6_addr, targetaddr_str, sizeof(targetaddr_str));
            printf("[%3d] local [%s]:%" PRIu16 " connected to [%s]:%" PRIu16 "\n",
                    instance_id,
                    localaddr_str, ntohs(local_addr6.sin6_port),
                    targetaddr_str, ntohs(target_addr6->sin6_port));
        }
#endif
    }
}

static void iperf_print_traffic_header()
{
    printf("\n[ ID] Interval\t\tTransfer\tBandwidth\n");
}

static void iperf_print_traffic_report(const iperf_report_t* report)
{
    double transfer = 0.0;
    double bandwidth = 0.0;
    char format_ch = '\0';

    /* period or summary */
    uint64_t data_bytes = report->traffic.period_bytes;
    int start_sec =  report->traffic.period_start_sec;
    int end_sec = report->traffic.end_sec;
    if (report->report_type == IPERF_REPORT_SUMMARY) {
        data_bytes = report->traffic.total_transfer_bytes;
        start_sec = 0;
    }

    if (end_sec - start_sec <= 0) {
        /* No traffic report collected */
        ESP_LOGD(TAG, "no traffic report: id=%d", report->instance_id);
        return;
    }

    switch (report->traffic.output_format) {
        case BITS_PER_SEC:
            transfer = data_bytes;
            break;
        case KBITS_PER_SEC: /* '-f k' */
            transfer = data_bytes / 1000.0;
            format_ch = 'K';
            break;
        case MBITS_PER_SEC: /* '-f m' */
            transfer = data_bytes / 1000.0 / 1000.0;
            format_ch = 'M';
            break;
        default:
            /* should not happen */
            break;
    }
    bandwidth = transfer / (end_sec - start_sec) * 8;

    printf("[%3d] %2" PRIu32 ".0-%2" PRIu32 ".0 sec\t%2.2f %cBytes\t%.2f %cbits/sec\n",
        report->instance_id,
        start_sec,
        end_sec,
        transfer,
        format_ch,
        bandwidth,
        format_ch);
}

void iperf_default_report_output(const iperf_report_t* report)
{
    switch (report->report_type) {
    case IPERF_REPORT_CONNECT_INFO:
        iperf_print_connect_info(report);
        iperf_print_traffic_header();
        break;
    case IPERF_REPORT_PERIOD:
        /* fallthrough */
    case IPERF_REPORT_SUMMARY:
        iperf_print_traffic_report(report);
        break;
    default:
        break;
    }
}

void iperf_report_task(void *arg)
{
    iperf_instance_data_t *iperf_instance = (iperf_instance_data_t *) arg;
    uint32_t data_len; /* period_data_snapshot is uint32_t */
    bool connnect_info_printed = false;
    iperf_report_t report;

    do {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (!connnect_info_printed) {
            iperf_copy_report(IPERF_REPORT_CONNECT_INFO, iperf_instance, &report);
            iperf_report_output(&report);
            connnect_info_printed = true;
        }
        iperf_report_task_data_t report_task_data_tmp = { 0 };
        // load a local copy and zero the atomic var to indicate report has been processed
        report_task_data_tmp = atomic_exchange(&iperf_instance->report_task_data, report_task_data_tmp);

        // check if valid data is available
        if (report_task_data_tmp.period_sec != 0) {
            data_len = report_task_data_tmp.period_data_snapshot;
            iperf_instance->traffic.period_bytes = data_len;
            iperf_instance->traffic.total_transfer_bytes += data_len;
            iperf_instance->traffic.period_start_sec = iperf_instance->traffic.end_sec;
            iperf_instance->traffic.end_sec += report_task_data_tmp.period_sec;
            /* IPERF_RUNNING to the state handler */
            iperf_state_action(IPERF_RUNNING, iperf_instance);
            iperf_copy_report(IPERF_REPORT_PERIOD, iperf_instance, &report);
            iperf_report_output(&report);
        }
    } while (iperf_instance->is_running);

    if (iperf_instance->traffic.end_sec != 0) {
        iperf_copy_report(IPERF_REPORT_SUMMARY, iperf_instance, &report);
        iperf_report_output(&report);
    }

    ESP_LOGD(TAG, "report (id=%d) task exited", iperf_instance->id);
    iperf_instance->report_task_hdl = NULL;
    vTaskDelete(NULL);
}


TaskHandle_t iperf_create_report_task(iperf_instance_data_t *iperf_instance)
{
    BaseType_t xResult = xTaskCreate(iperf_report_task, IPERF_REPORT_TASK_NAME, IPERF_REPORT_TASK_STACK,
                                     (void*)iperf_instance, IPERF_REPORT_TASK_PRIORITY, &(iperf_instance->report_task_hdl));
    if (xResult != pdPASS) {
        return NULL;
    }
    return iperf_instance->report_task_hdl;
}


/* weak function */
IPERF_WEAK_ATTR void iperf_report_output(const iperf_report_t* report)
{
    iperf_default_report_output(report);
}

/* public API, not thread safe */
esp_err_t iperf_get_traffic_report(iperf_id_t id, iperf_traffic_report_t *traffic)
{
    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_FALSE(id >= 0, ESP_ERR_INVALID_ARG, err, TAG, "invalid id provided (id=%" PRIi8 ")", id);
    ESP_GOTO_ON_FALSE(traffic != NULL, ESP_ERR_INVALID_ARG, err, TAG, "invalid pointer to report");
    iperf_instance_data_t *iperf_instance = iperf_list_get_instance_by_id(id);
    ESP_GOTO_ON_FALSE(iperf_instance != NULL, ESP_ERR_INVALID_STATE, err, TAG, "cannot get data: instance (id=%" PRIi8 ") was not found", id);
    memcpy(traffic, &(iperf_instance->traffic), sizeof(iperf_traffic_report_t));
err:
    return ret;
}
