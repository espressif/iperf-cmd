/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "iperf.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Sets the global default iperf state handler. This handler will be passed to each iperf instance as a configuration parameter.
 *
 * @note This function should be called before starting the console.
 *
 * @param state_hndl state handler function to process runtime details from iperf
 * @param priv pointer to user's private data later passed to state handler function
 * @return
 *      - ESP_OK on success
 */
esp_err_t iperf_cmd_set_iperf_state_handler(iperf_state_handler_func_t state_hndl, void *priv);

/**
 * @brief Registers console commands: iperf.
 *
 * @return
 *      - ESP_OK on success
 */
esp_err_t iperf_cmd_register_iperf(void);

#ifdef __cplusplus
}
#endif
