/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <errno.h>
#include <string.h>
#include <sys/socket.h>

/* Build check: LWIP_IPV4/LWIP_IPV6 should be defined after include sys/socket.h */
#if !(defined LWIP_IPV4) || !(defined LWIP_IPV6)
#error "Both LWIP_IPV4 and LWIP_IPV6 should be defined!"
#endif

#include "esp_log.h"
#include "esp_err.h"
#include "esp_console.h"
#include "esp_netif.h"

#include "iperf_cmd.h"

void my_report_func()
{
    static int i  = 0;
    if (i == 0 ) {
        printf("my_report_func was called\n");
        i++;
    }
}

/* override weak func */
void iperf_report_output(const iperf_report_t* report)
{
    my_report_func();
    iperf_default_report_output(report);
}

void app_main(void)
{

    ESP_ERROR_CHECK(esp_netif_init());

    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    repl_config.prompt = "iperf>";
    repl_config.max_history_len = 1;
    repl_config.task_priority = 2;
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));

    /* Register iperf command */
    ESP_ERROR_CHECK(iperf_cmd_register_iperf());

    // start console REPL
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
