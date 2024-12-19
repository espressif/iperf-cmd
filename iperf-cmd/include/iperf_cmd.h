/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "iperf.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Register user's report callback function.
 *
 * @note Registered function is then passed to iperf instances as configuration parameter at their startup.
 *
 * @param report_hndl report handler function to process runtime details from iperf
 * @param priv pointer to user's private data later passed to report function
 * @return
 *      - ESP_OK on success
 */
esp_err_t iperf_cmd_register_report_handler(iperf_report_handler_func_t report_hndl, void *priv);

/**
 * @brief Registers console commands: iperf.
 *
 * @return ESP_OK on success
 */
esp_err_t iperf_cmd_register_iperf(void);

#ifdef __cplusplus
}
#endif
