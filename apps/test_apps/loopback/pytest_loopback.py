# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
import pytest

from pytest_embedded import Dut


@pytest.mark.target('esp32')
@pytest.mark.target('esp32c5')
@pytest.mark.env('generic')
def test_iperf_loopback(dut: Dut) -> None:
    dut.run_all_single_board_cases(timeout=90)
