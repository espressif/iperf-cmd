/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <inttypes.h>
#include <stdio.h>
#include "iperf.h"


void iperf_default_report_output(const iperf_report_t* data)
{
    float transfer = 0.0;
    float bandwidth = 0.0;
    int period_time = data->end_sec - data->start_sec;
    char format_ch = '\0';
    static bool is_iperf_header_displayed = false;

    switch (data->output_format) {
        case BITS_PER_SEC:
            transfer = data->period_bytes;
            break;
        case KBITS_PER_SEC:
            transfer = data->period_bytes / 1000.0;
            format_ch = 'K';
            break;
        case MBITS_PER_SEC:
            transfer = data->period_bytes / 1000.0 / 1000.0;
            format_ch = 'M';
            break;
        default:
            /* should not happen */
            break;
    }
    bandwidth = transfer / period_time * 8;

    // only prints the header once
    if (is_iperf_header_displayed == false) {
        printf("\n[ ID] Interval\t\tTransfer\tBandwidth\n");
        is_iperf_header_displayed = true;
    }

    printf("[%3d] %2" PRIu32 ".0-%2" PRIu32 ".0 sec\t%2.2f %cBytes\t%.2f %cbits/sec\n",
        data->instance_id,
        data->start_sec,
        data->end_sec,
        transfer,
        format_ch,
        bandwidth,
        format_ch);
}
