/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <arpa/inet.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_bit_defs.h"
#include "unity.h"

#include "iperf.h"

static const char *TAG = "iperf_test";

#define IPERF_SERVER_START_BIT     BIT(0)
#define IPERF_SERVER_STOP_BIT      BIT(1)
#define IPERF_SERVER_RUNNING_BIT   BIT(2)
#define IPERF_SERVER_CLOSE_BIT     BIT(3)
#define IPERF_CLIENT_START_BIT     BIT(4)
#define IPERF_CLIENT_STOP_BIT      BIT(5)
#define IPERF_CLIENT_RUNNING_BIT   BIT(6)
#define IPERF_CLIENT_CLOSE_BIT     BIT(7)

#define UDP_CLIENT_BW_FACTOR_ICMP  (0.4) // expected bandwidth to be ~40% (worst case) due to the ICMP "Port Unreachable" is sent

// older versions of unity don't have these types of test assertions
#ifndef TEST_ASSERT_GREATER_OR_EQUAL_FLOAT
#define TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(expected, actual) \
    TEST_ASSERT_TRUE((actual) >= (expected))
#endif
#ifndef TEST_ASSERT_GREATER_THAN_FLOAT
#define TEST_ASSERT_GREATER_THAN_FLOAT(expected, actual) \
    TEST_ASSERT_TRUE((actual) > (expected))
#endif

typedef struct {
    EventGroupHandle_t event_group;
    iperf_traffic_report_t server_report;
    iperf_traffic_report_t client_report;
} iperf_test_data_t;

static float average_bandwidth_from_report(iperf_traffic_report_t* traffic)
{
    float average_bandwidth = 0;
    if (traffic->end_sec == 0) {
        ESP_LOGE(TAG, "failed to get traffic report!");
        return average_bandwidth;
    }
    /* calculate bandwidth */
    switch (traffic->output_format) {
        case MBYTES_PER_SEC:
            average_bandwidth = traffic->total_transfer_bytes / traffic->end_sec / 1024.0 / 1024.0;
            break;
        case KBYTES_PER_SEC:
            average_bandwidth = traffic->total_transfer_bytes / traffic->end_sec / 1024.0;
            break;
        case MBITS_PER_SEC:
            average_bandwidth = traffic->total_transfer_bytes / traffic->end_sec / 1000.0 / 1000.0 * 8;
            break;
        case KBITS_PER_SEC:
            average_bandwidth = traffic->total_transfer_bytes / traffic->end_sec / 1000.0 * 8;
            break;
        case BITS_PER_SEC:
            average_bandwidth = traffic->total_transfer_bytes / traffic->end_sec * 8;
            break;
        default:
            /* never happen */
            ESP_LOGE(TAG, "can not calculate bandwidth for format: %d", traffic->output_format);
            break;
    }
    return average_bandwidth;
}

static const char *get_unit_string_from_format(iperf_output_format_t format)
{
    switch (format) {
        case MBYTES_PER_SEC:
            return "MBps";
        case KBYTES_PER_SEC:
            return "KBps";
        case MBITS_PER_SEC:
            return "Mbps";
        case KBITS_PER_SEC:
            return "Kbps";
        case BITS_PER_SEC:
            return "bps";
        default:
            /* never happen */
            ESP_LOGE(TAG, "can not get unit string for format: %d", format);
            return "";
    }
}

static void iperf_server_state_cb(iperf_id_t id, iperf_state_data_t* data, void *priv)
{
    iperf_test_data_t *iperf_data = (iperf_test_data_t *)priv;
    EventGroupHandle_t event_group = iperf_data->event_group;
    switch (data->state)
    {
    case IPERF_STARTED:
        xEventGroupSetBits(event_group, IPERF_SERVER_START_BIT);
        break;
    case IPERF_STOPPED:
        xEventGroupSetBits(event_group, IPERF_SERVER_STOP_BIT);
        break;
    case IPERF_RUNNING:
        xEventGroupSetBits(event_group, IPERF_SERVER_RUNNING_BIT);
        break;
    case IPERF_CLOSED:
        iperf_get_traffic_report(id, &iperf_data->server_report);
        xEventGroupSetBits(event_group, IPERF_SERVER_CLOSE_BIT);
        break;
    default:
        break;
    }
}

static void iperf_client_state_cb(iperf_id_t id, iperf_state_data_t* data, void *priv)
{
    iperf_test_data_t *iperf_data = (iperf_test_data_t *)priv;
    EventGroupHandle_t event_group = iperf_data->event_group;
    switch (data->state)
    {
    case IPERF_STARTED:
        xEventGroupSetBits(event_group, IPERF_CLIENT_START_BIT);
        break;
    case IPERF_STOPPED:
        xEventGroupSetBits(event_group, IPERF_CLIENT_STOP_BIT);
        break;
    case IPERF_RUNNING:
        xEventGroupSetBits(event_group, IPERF_CLIENT_RUNNING_BIT);
        break;
    case IPERF_CLOSED:
        iperf_get_traffic_report(id, &iperf_data->client_report);
        xEventGroupSetBits(event_group, IPERF_CLIENT_CLOSE_BIT);
        break;
    default:
        break;
    }
}

static void udp_basic_test(esp_ip_addr_t esp_addr_any, esp_ip_addr_t esp_addr_loopback, esp_ip_addr_t esp_addr_err)
{
    TEST_ESP_OK(esp_event_loop_create_default());
    TEST_ESP_OK(esp_netif_init());

    EventGroupHandle_t iperf_event_group = xEventGroupCreate();
    TEST_ASSERT(iperf_event_group != NULL);

    iperf_test_data_t iperf_data = {
        .event_group = iperf_event_group
    };

    EventBits_t server_all_states_bits = IPERF_SERVER_START_BIT | IPERF_SERVER_STOP_BIT | IPERF_SERVER_RUNNING_BIT | IPERF_SERVER_CLOSE_BIT;
    EventBits_t client_all_states_bits = IPERF_CLIENT_START_BIT | IPERF_CLIENT_STOP_BIT | IPERF_CLIENT_RUNNING_BIT | IPERF_CLIENT_CLOSE_BIT;

    ESP_LOGI(TAG, "--------------");
    ESP_LOGI(TAG, "UDP basic test");
    ESP_LOGI(TAG, "--------------");
    iperf_cfg_t udp_server_cfg = IPERF_DEFAULT_CONFIG_SERVER(IPERF_FLAG_UDP, esp_addr_any);
    udp_server_cfg.state_handler = iperf_server_state_cb;
    udp_server_cfg.state_handler_priv = &iperf_data;
    udp_server_cfg.time = 5;
    iperf_cfg_t udp_client_cfg = IPERF_DEFAULT_CONFIG_CLIENT(IPERF_FLAG_UDP, esp_addr_loopback);
    udp_client_cfg.state_handler = iperf_client_state_cb;
    udp_client_cfg.state_handler_priv = &iperf_data;
    udp_client_cfg.time = 5;

    memset(&iperf_data.client_report, 0, sizeof(iperf_traffic_report_t));
    memset(&iperf_data.server_report, 0, sizeof(iperf_traffic_report_t));

    iperf_id_t server_id = iperf_start_instance(&udp_server_cfg);
    TEST_ASSERT_EQUAL_INT8(1, server_id);
    iperf_id_t client_id = iperf_start_instance(&udp_client_cfg);
    TEST_ASSERT_EQUAL_INT8(2, client_id);
    EventBits_t bits = xEventGroupWaitBits(iperf_event_group, server_all_states_bits | client_all_states_bits,
                               pdTRUE, pdTRUE, pdMS_TO_TICKS(10000));
    ESP_LOGI(TAG, "bits: 0x%lx", bits);
    TEST_ASSERT_BITS_HIGH(server_all_states_bits | client_all_states_bits, bits);

    const char *unit_str = get_unit_string_from_format(iperf_data.client_report.output_format);

    ESP_LOGI(TAG, "average client throughput: %.2lf %s", average_bandwidth_from_report(&(iperf_data.client_report)), unit_str);
    ESP_LOGI(TAG, "average server throughput: %.2lf %s", average_bandwidth_from_report(&(iperf_data.server_report)), unit_str);
    TEST_ASSERT_GREATER_THAN_FLOAT(0.1, average_bandwidth_from_report(&(iperf_data.client_report)));
    TEST_ASSERT_GREATER_THAN_FLOAT(0.1, average_bandwidth_from_report(&(iperf_data.server_report)));
    TEST_ASSERT_FLOAT_WITHIN(0.5, average_bandwidth_from_report(&(iperf_data.client_report)), average_bandwidth_from_report(&(iperf_data.server_report)));

    ESP_LOGI(TAG, "-----------------");
    ESP_LOGI(TAG, "UDP bind - client");
    ESP_LOGI(TAG, "-----------------");
    vTaskDelay(pdMS_TO_TICKS(100)); // invoke context switch to iperf finishes its closure (to get expected instance IDs, note that it doesn't matter in real life)
    udp_client_cfg.source = esp_addr_loopback;
    udp_client_cfg.sport = 6666;

    memset(&iperf_data.client_report, 0, sizeof(iperf_traffic_report_t));
    memset(&iperf_data.server_report, 0, sizeof(iperf_traffic_report_t));

    server_id = iperf_start_instance(&udp_server_cfg);
    TEST_ASSERT_EQUAL_INT8(1, server_id);
    client_id = iperf_start_instance(&udp_client_cfg);
    TEST_ASSERT_EQUAL_INT8(2, client_id);
    bits = xEventGroupWaitBits(iperf_event_group, server_all_states_bits | client_all_states_bits,
                               pdTRUE, pdTRUE, pdMS_TO_TICKS(10000));
    ESP_LOGI(TAG, "bits: 0x%lx", bits);
    TEST_ASSERT_BITS_HIGH(server_all_states_bits | client_all_states_bits, bits);

    ESP_LOGI(TAG, "average client throughput: %.2lf %s", average_bandwidth_from_report(&(iperf_data.client_report)), unit_str);
    ESP_LOGI(TAG, "average server throughput: %.2lf %s", average_bandwidth_from_report(&(iperf_data.server_report)), unit_str);
    TEST_ASSERT_GREATER_THAN_FLOAT(0.1, average_bandwidth_from_report(&(iperf_data.client_report)));
    TEST_ASSERT_GREATER_THAN_FLOAT(0.1, average_bandwidth_from_report(&(iperf_data.server_report)));
    TEST_ASSERT_FLOAT_WITHIN(0.5, average_bandwidth_from_report(&(iperf_data.client_report)), average_bandwidth_from_report(&(iperf_data.server_report)));

    ESP_LOGI(TAG, "-----------------");
    ESP_LOGI(TAG, "UDP bind - server");
    ESP_LOGI(TAG, "-----------------");
    vTaskDelay(pdMS_TO_TICKS(100)); // invoke context switch to iperf finishes its closure (to get expected instance IDs, note that it doesn't matter in real life)
    udp_server_cfg.source = esp_addr_loopback;
    udp_client_cfg.source = esp_addr_loopback;
    udp_client_cfg.sport = 6666;

    memset(&iperf_data.client_report, 0, sizeof(iperf_traffic_report_t));
    memset(&iperf_data.server_report, 0, sizeof(iperf_traffic_report_t));

    server_id = iperf_start_instance(&udp_server_cfg);
    TEST_ASSERT_EQUAL_INT8(1, server_id);
    client_id = iperf_start_instance(&udp_client_cfg);
    TEST_ASSERT_EQUAL_INT8(2, client_id);
    bits = xEventGroupWaitBits(iperf_event_group, server_all_states_bits | client_all_states_bits,
                               pdTRUE, pdTRUE, pdMS_TO_TICKS(10000));
    ESP_LOGI(TAG, "bits: 0x%lx", bits);
    TEST_ASSERT_BITS_HIGH(server_all_states_bits | client_all_states_bits, bits);

    ESP_LOGI(TAG, "average client throughput: %.2lf %s", average_bandwidth_from_report(&(iperf_data.client_report)), unit_str);
    ESP_LOGI(TAG, "average server throughput: %.2lf %s", average_bandwidth_from_report(&(iperf_data.server_report)), unit_str);
    TEST_ASSERT_GREATER_THAN_FLOAT(0.1, average_bandwidth_from_report(&(iperf_data.client_report)));
    TEST_ASSERT_GREATER_THAN_FLOAT(0.1, average_bandwidth_from_report(&(iperf_data.server_report)));
    TEST_ASSERT_FLOAT_WITHIN(0.5, average_bandwidth_from_report(&(iperf_data.client_report)), average_bandwidth_from_report(&(iperf_data.server_report)));

    ESP_LOGI(TAG, "------------------------------------------------------------");
    ESP_LOGI(TAG, "bind server to address which differs to client's destination");
    ESP_LOGI(TAG, "------------------------------------------------------------");
    vTaskDelay(pdMS_TO_TICKS(100)); // invoke context switch to iperf finishes its closure (to get expected instance IDs, note that it doesn't matter in real life)
    // => server does not receive any data
    udp_server_cfg.source = esp_addr_err;

    // store the previous bandwidth to get idea what to expect
    float average_bw_prev = average_bandwidth_from_report(&(iperf_data.client_report));
    memset(&iperf_data.client_report, 0, sizeof(iperf_traffic_report_t));
    memset(&iperf_data.server_report, 0, sizeof(iperf_traffic_report_t));

    server_id = iperf_start_instance(&udp_server_cfg);
    TEST_ASSERT_EQUAL_INT8(1, server_id);
    client_id = iperf_start_instance(&udp_client_cfg);
    TEST_ASSERT_EQUAL_INT8(2, client_id);
    bits = xEventGroupWaitBits(iperf_event_group, server_all_states_bits | client_all_states_bits,
                               pdTRUE, pdTRUE, pdMS_TO_TICKS(12000));
    ESP_LOGI(TAG, "bits: 0x%lx", bits);
    // server just indicates it's closed (never starts/stops receiving data)
    TEST_ASSERT_BITS(server_all_states_bits | client_all_states_bits, IPERF_SERVER_CLOSE_BIT | client_all_states_bits, bits);
    // previously timeouted, hence bits need to be cleared
    xEventGroupClearBits(iperf_event_group, server_all_states_bits | client_all_states_bits);

    ESP_LOGI(TAG, "average client throughput: %.2lf %s", average_bandwidth_from_report(&(iperf_data.client_report)), unit_str);
    ESP_LOGI(TAG, "average server throughput: %.2lf %s", average_bandwidth_from_report(&(iperf_data.server_report)), unit_str);
    TEST_ASSERT_EQUAL_FLOAT(0, average_bandwidth_from_report(&(iperf_data.server_report)));
    TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(average_bw_prev * UDP_CLIENT_BW_FACTOR_ICMP, average_bandwidth_from_report(&(iperf_data.client_report))); // performance is expected to be less due to the ICMP "Port Unreachable" is sent

    ESP_LOGI(TAG, "----------------------");
    ESP_LOGI(TAG, "start the client first");
    ESP_LOGI(TAG, "----------------------");
    vTaskDelay(pdMS_TO_TICKS(100)); // invoke context switch to iperf finishes its closure (to get expected instance IDs, note that it doesn't matter in real life)
    udp_server_cfg.source = esp_addr_any;

    memset(&iperf_data.client_report, 0, sizeof(iperf_traffic_report_t));
    memset(&iperf_data.server_report, 0, sizeof(iperf_traffic_report_t));

    udp_client_cfg.instance_id = 3;
    udp_server_cfg.instance_id = 5;
    client_id = iperf_start_instance(&udp_client_cfg);
    TEST_ASSERT_EQUAL_INT8(3, client_id);
    // can delay since client does not wait for connection
    vTaskDelay(pdMS_TO_TICKS(1000));
    server_id = iperf_start_instance(&udp_server_cfg);
    TEST_ASSERT_EQUAL_INT8(5, server_id);
    bits = xEventGroupWaitBits(iperf_event_group, server_all_states_bits | client_all_states_bits,
                               pdTRUE, pdTRUE, pdMS_TO_TICKS(10000));
    ESP_LOGI(TAG, "bits: 0x%lx", bits);
    TEST_ASSERT_BITS_HIGH(server_all_states_bits | client_all_states_bits, bits);

    ESP_LOGI(TAG, "average client throughput: %.2lf %s", average_bandwidth_from_report(&(iperf_data.client_report)), unit_str);
    ESP_LOGI(TAG, "average server throughput: %.2lf %s", average_bandwidth_from_report(&(iperf_data.server_report)), unit_str);
    // performance may be also affected by the ICMP "Port Unreachable" since server is started later
    TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(average_bw_prev * UDP_CLIENT_BW_FACTOR_ICMP, average_bandwidth_from_report(&(iperf_data.client_report)));
    TEST_ASSERT_GREATER_THAN_FLOAT(5, average_bandwidth_from_report(&(iperf_data.server_report))); // server is started later, hence avg. bw is expected to be low

    TEST_ESP_OK(esp_event_loop_delete_default());
    vEventGroupDelete(iperf_event_group);
    vTaskDelay(pdMS_TO_TICKS(100)); // invoke context switch to iperf finishes its closure (to get expected instance IDs, note that it doesn't matter in real life)
}

static void tcp_basic_test(esp_ip_addr_t esp_addr_any, esp_ip_addr_t esp_addr_loopback, esp_ip_addr_t esp_addr_err)
{
    TEST_ESP_OK(esp_event_loop_create_default());
    TEST_ESP_OK(esp_netif_init());

    EventGroupHandle_t iperf_event_group = xEventGroupCreate();
    TEST_ASSERT(iperf_event_group != NULL);

    iperf_test_data_t iperf_data = {
        .event_group = iperf_event_group
    };

    EventBits_t server_all_states_bits = IPERF_SERVER_START_BIT | IPERF_SERVER_STOP_BIT | IPERF_SERVER_RUNNING_BIT | IPERF_SERVER_CLOSE_BIT;
    EventBits_t client_all_states_bits = IPERF_CLIENT_START_BIT | IPERF_CLIENT_STOP_BIT | IPERF_CLIENT_RUNNING_BIT | IPERF_CLIENT_CLOSE_BIT;

    ESP_LOGI(TAG, "-----------");
    ESP_LOGI(TAG, "TCP - basic");
    ESP_LOGI(TAG, "-----------");
    iperf_cfg_t udp_server_cfg = IPERF_DEFAULT_CONFIG_SERVER(IPERF_FLAG_TCP, esp_addr_any);
    udp_server_cfg.state_handler = iperf_server_state_cb;
    udp_server_cfg.state_handler_priv = &iperf_data;
    udp_server_cfg.time = 5;
    iperf_cfg_t udp_client_cfg = IPERF_DEFAULT_CONFIG_CLIENT(IPERF_FLAG_TCP, esp_addr_loopback);
    udp_client_cfg.state_handler = iperf_client_state_cb;
    udp_client_cfg.state_handler_priv = &iperf_data;
    udp_client_cfg.time = 5;

    memset(&iperf_data.client_report, 0, sizeof(iperf_traffic_report_t));
    memset(&iperf_data.server_report, 0, sizeof(iperf_traffic_report_t));

    iperf_id_t server_id = iperf_start_instance(&udp_server_cfg);
    TEST_ASSERT_EQUAL_INT8(1, server_id);
    vTaskDelay(pdMS_TO_TICKS(100)); // invoke context switch to lwIP can create server's listen socket
    iperf_id_t client_id = iperf_start_instance(&udp_client_cfg);
    TEST_ASSERT_EQUAL_INT8(2, client_id);
    EventBits_t bits = xEventGroupWaitBits(iperf_event_group, server_all_states_bits | client_all_states_bits,
                               pdTRUE, pdTRUE, pdMS_TO_TICKS(10000));
    ESP_LOGI(TAG, "bits: 0x%lx", bits);
    TEST_ASSERT_BITS_HIGH(server_all_states_bits | client_all_states_bits, bits);

    const char *unit_str = get_unit_string_from_format(iperf_data.client_report.output_format);

    ESP_LOGI(TAG, "average client throughput: %.2lf %s\n", average_bandwidth_from_report(&(iperf_data.client_report)), unit_str);
    ESP_LOGI(TAG, "average server throughput: %.2lf %s", average_bandwidth_from_report(&(iperf_data.server_report)), unit_str);
    TEST_ASSERT_GREATER_THAN_FLOAT(0.1, average_bandwidth_from_report(&(iperf_data.client_report)));
    TEST_ASSERT_GREATER_THAN_FLOAT(0.1, average_bandwidth_from_report(&(iperf_data.server_report)));
    TEST_ASSERT_FLOAT_WITHIN(0.5, average_bandwidth_from_report(&(iperf_data.client_report)), average_bandwidth_from_report(&(iperf_data.server_report)));

    ESP_LOGI(TAG, "-----------------");
    ESP_LOGI(TAG, "TCP bind - client");
    ESP_LOGI(TAG, "-----------------");
    vTaskDelay(pdMS_TO_TICKS(100)); // invoke context switch to iperf finishes its closure (to get expected instance IDs, note that it doesn't matter in real life)
    udp_client_cfg.source = esp_addr_loopback;
    udp_client_cfg.sport = 6666;

    memset(&iperf_data.client_report, 0, sizeof(iperf_traffic_report_t));
    memset(&iperf_data.server_report, 0, sizeof(iperf_traffic_report_t));

    server_id = iperf_start_instance(&udp_server_cfg);
    TEST_ASSERT_EQUAL_INT8(1, server_id);
    vTaskDelay(pdMS_TO_TICKS(100)); // invoke context switch to lwIP can create server's listen socket
    client_id = iperf_start_instance(&udp_client_cfg);
    TEST_ASSERT_EQUAL_INT8(2, client_id);
    bits = xEventGroupWaitBits(iperf_event_group, server_all_states_bits | client_all_states_bits,
                               pdTRUE, pdTRUE, pdMS_TO_TICKS(10000));
    ESP_LOGI(TAG, "bits: 0x%lx", bits);
    TEST_ASSERT_BITS_HIGH(server_all_states_bits | client_all_states_bits, bits);

    ESP_LOGI(TAG, "average client throughput: %.2lf %s", average_bandwidth_from_report(&(iperf_data.client_report)), unit_str);
    ESP_LOGI(TAG, "average server throughput: %.2lf %s", average_bandwidth_from_report(&(iperf_data.server_report)), unit_str);
    TEST_ASSERT_GREATER_THAN_FLOAT(0.1, average_bandwidth_from_report(&(iperf_data.client_report)));
    TEST_ASSERT_GREATER_THAN_FLOAT(0.1, average_bandwidth_from_report(&(iperf_data.server_report)));
    TEST_ASSERT_FLOAT_WITHIN(0.5, average_bandwidth_from_report(&(iperf_data.client_report)), average_bandwidth_from_report(&(iperf_data.server_report)));

    ESP_LOGI(TAG, "-----------------");
    ESP_LOGI(TAG, "TCP bind - server");
    ESP_LOGI(TAG, "-----------------");
    vTaskDelay(pdMS_TO_TICKS(100)); // invoke context switch to iperf finishes its closure (to get expected instance IDs, note that it doesn't matter in real life)
    udp_server_cfg.source = esp_addr_loopback;
    udp_client_cfg.source = esp_addr_loopback;
    udp_client_cfg.sport = 6666;

    memset(&iperf_data.client_report, 0, sizeof(iperf_traffic_report_t));
    memset(&iperf_data.server_report, 0, sizeof(iperf_traffic_report_t));

    server_id = iperf_start_instance(&udp_server_cfg);
    TEST_ASSERT_EQUAL_INT8(1, server_id);
    vTaskDelay(pdMS_TO_TICKS(100)); // invoke context switch to lwIP can create server's listen socket
    client_id = iperf_start_instance(&udp_client_cfg);
    TEST_ASSERT_EQUAL_INT8(2, client_id);
    bits = xEventGroupWaitBits(iperf_event_group, server_all_states_bits | client_all_states_bits,
                               pdTRUE, pdTRUE, pdMS_TO_TICKS(10000));
    ESP_LOGI(TAG, "bits: 0x%lx", bits);
    TEST_ASSERT_BITS_HIGH(server_all_states_bits | client_all_states_bits, bits);

    ESP_LOGI(TAG, "average client throughput: %.2lf %s", average_bandwidth_from_report(&(iperf_data.client_report)), unit_str);
    ESP_LOGI(TAG, "average server throughput: %.2lf %s", average_bandwidth_from_report(&(iperf_data.server_report)), unit_str);
    TEST_ASSERT_GREATER_THAN_FLOAT(0.1, average_bandwidth_from_report(&(iperf_data.client_report)));
    TEST_ASSERT_GREATER_THAN_FLOAT(0.1, average_bandwidth_from_report(&(iperf_data.server_report)));
    TEST_ASSERT_FLOAT_WITHIN(0.5, average_bandwidth_from_report(&(iperf_data.client_report)), average_bandwidth_from_report(&(iperf_data.server_report)));

    ESP_LOGI(TAG, "------------------------------------------------------------");
    ESP_LOGI(TAG, "bind server to address which differs to client's destination");
    ESP_LOGI(TAG, "------------------------------------------------------------");
    vTaskDelay(pdMS_TO_TICKS(100)); // invoke context switch to iperf finishes its closure (to get expected instance IDs, note that it doesn't matter in real life)
    // => client cannot connect
    udp_server_cfg.source = esp_addr_err;

    memset(&iperf_data.client_report, 0, sizeof(iperf_traffic_report_t));
    memset(&iperf_data.server_report, 0, sizeof(iperf_traffic_report_t));

    server_id = iperf_start_instance(&udp_server_cfg);
    TEST_ASSERT_EQUAL_INT8(1, server_id);
    vTaskDelay(pdMS_TO_TICKS(100)); // invoke context switch to lwIP can create server's listen socket
    client_id = iperf_start_instance(&udp_client_cfg);
    TEST_ASSERT_EQUAL_INT8(2, client_id);
    bits = xEventGroupWaitBits(iperf_event_group, IPERF_SERVER_CLOSE_BIT | IPERF_CLIENT_CLOSE_BIT,
                               pdTRUE, pdTRUE, pdMS_TO_TICKS(12000));
    ESP_LOGI(TAG, "bits: 0x%lx", bits);
    // expect exactly only IPERF_SERVER_CLOSE_BIT | IPERF_CLIENT_CLOSE_BIT
    TEST_ASSERT_BITS(server_all_states_bits | client_all_states_bits, IPERF_SERVER_CLOSE_BIT | IPERF_CLIENT_CLOSE_BIT, bits);

    ESP_LOGI(TAG, "average client throughput: %.2lf %s", average_bandwidth_from_report(&(iperf_data.client_report)), unit_str);
    ESP_LOGI(TAG, "average server throughput: %.2lf %s", average_bandwidth_from_report(&(iperf_data.server_report)), unit_str);
    TEST_ASSERT_EQUAL_FLOAT(0, average_bandwidth_from_report(&(iperf_data.server_report)));
    TEST_ASSERT_EQUAL_FLOAT(0, average_bandwidth_from_report(&(iperf_data.client_report)));

    ESP_LOGI(TAG, "----------------------");
    ESP_LOGI(TAG, "start the client first");
    ESP_LOGI(TAG, "----------------------");
    vTaskDelay(pdMS_TO_TICKS(100)); // invoke context switch to iperf finishes its closure (to get expected instance IDs, note that it doesn't matter in real life)
    udp_server_cfg.source = esp_addr_any;

    memset(&iperf_data.client_report, 0, sizeof(iperf_traffic_report_t));
    memset(&iperf_data.server_report, 0, sizeof(iperf_traffic_report_t));

    client_id = iperf_start_instance(&udp_client_cfg);
    TEST_ASSERT_EQUAL_INT8(1, client_id);
    server_id = iperf_start_instance(&udp_server_cfg);
    // Note: don't check for server_id, it can be 1 since client may almost immediately return

    bits = xEventGroupWaitBits(iperf_event_group, IPERF_SERVER_CLOSE_BIT | IPERF_CLIENT_CLOSE_BIT,
                               pdTRUE, pdTRUE, pdMS_TO_TICKS(11000));
    ESP_LOGI(TAG, "bits: 0x%lx", bits);
    TEST_ASSERT_BITS_HIGH(IPERF_SERVER_CLOSE_BIT | IPERF_CLIENT_CLOSE_BIT, bits);

    ESP_LOGI(TAG, "average client throughput: %.2lf %s", average_bandwidth_from_report(&(iperf_data.client_report)), unit_str);
    ESP_LOGI(TAG, "average server throughput: %.2lf %s", average_bandwidth_from_report(&(iperf_data.server_report)), unit_str);
    TEST_ASSERT_EQUAL_FLOAT(0, average_bandwidth_from_report(&(iperf_data.client_report)));
    TEST_ASSERT_EQUAL_FLOAT(0, average_bandwidth_from_report(&(iperf_data.server_report)));

    TEST_ESP_OK(esp_event_loop_delete_default());
    vEventGroupDelete(iperf_event_group);
    vTaskDelay(pdMS_TO_TICKS(100)); // invoke context switch to iperf finishes its closure (to get expected instance IDs, note that it doesn't matter in real life)
}

TEST_CASE("iperf - UDP IPv4 basics", "[iperf]")
{
    esp_ip_addr_t esp_addr4_any = ESP_IP4ADDR_INIT(0, 0, 0, 0);
    esp_ip_addr_t esp_addr4_loopback = ESP_IP4ADDR_INIT(127, 0, 0, 1);
    esp_ip_addr_t esp_addr4_err = ESP_IP4ADDR_INIT(10, 0, 0, 1);
    udp_basic_test(esp_addr4_any, esp_addr4_loopback, esp_addr4_err);
}

TEST_CASE("iperf - UDP IPv6 basics", "[iperf]")
{
    esp_ip_addr_t esp_addr6_any = ESP_IP6ADDR_INIT(0, 0, 0, 0);
    esp_ip_addr_t esp_addr6_loopback = ESP_IP6ADDR_INIT(0, 0, 0, htonl(1)); // IDF-11820
    esp_ip_addr_t esp_addr6_err = ESP_IP6ADDR_INIT(htonl(0xfd00), 0, 0, htonl(1));
    udp_basic_test(esp_addr6_any, esp_addr6_loopback, esp_addr6_err);
}

TEST_CASE("iperf - TCP IPv4 basics", "[iperf]")
{
    esp_ip_addr_t esp_addr4_any = ESP_IP4ADDR_INIT(0, 0, 0, 0);
    esp_ip_addr_t esp_addr4_loopback = ESP_IP4ADDR_INIT(127, 0, 0, 1);
    esp_ip_addr_t esp_addr4_err = ESP_IP4ADDR_INIT(10, 0, 0, 1);
    tcp_basic_test(esp_addr4_any, esp_addr4_loopback, esp_addr4_err);
}

TEST_CASE("iperf - TCP IPv6 basics", "[iperf]")
{
    esp_ip_addr_t esp_addr6_any = ESP_IP6ADDR_INIT(0, 0, 0, 0);
    esp_ip_addr_t esp_addr6_loopback = ESP_IP6ADDR_INIT(0, 0, 0, htonl(1));
    esp_ip_addr_t esp_addr6_err = ESP_IP6ADDR_INIT(htonl(0xfd00), 0, 0, htonl(1));
    tcp_basic_test(esp_addr6_any, esp_addr6_loopback, esp_addr6_err);
}

void app_main(void)
{
    vTaskPrioritySet(NULL, 10); // set higher than iperf default to not affect test flow
    unity_run_menu();
}
