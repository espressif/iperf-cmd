/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Registers the iperf commands.
 *
 * @return ESP_OK on success
 */
esp_err_t app_register_iperf_commands(void);

#ifdef __cplusplus
}
#endif
