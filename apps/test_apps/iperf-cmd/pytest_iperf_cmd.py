# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
import os
import time

from ast import literal_eval
from typing import Dict

import pytest

from pytest_embedded import Dut

# Default throughput standard, in Mbits/sec, 3~10M less than result
TEST_BW_STANDARD: Dict[str, Dict[str, float]] = {
    'esp32': {
        'tcp': 22,
        'udp': 26,
        'tcpv6': 16,
    },
    'esp32c5': {
        'tcp': 62,
        'udp': 80,
        'tcpv6': 60,
    },
}
if os.getenv('TEST_BW_STANDARD'):
    TEST_BW_STANDARD = literal_eval(os.environ['TEST_BW_STANDARD'])


def _bw_standard(chip: str, proto: str) -> float:
    try:
        return TEST_BW_STANDARD[chip][proto]
    except IndexError:
        print(f'Failed to get standard {chip}/{proto}')
        return 10


@pytest.mark.target('esp32')
@pytest.mark.target('esp32c5')
@pytest.mark.env('generic')
def test_iperf_cmd(dut: Dut) -> None:
    dut.expect('iperf>')
    time.sleep(1)

    # tcp
    dut.write('iperf -s -i 1 -t 9 --id=1')
    dut.write('iperf -c 127.0.0.1 -i 1 -t 9 --id=2')
    # delimiter: '\t'
    match1 = dut.expect(r'\[\s*([12])\]\s+1.0- 2.0 sec\s+([\d\.]+) MBytes\s+([\d\.]+) Mbits/sec')
    assert float(match1[2]) > _bw_standard(dut.target, 'tcp') / 8  # MBytes
    assert float(match1[3]) > _bw_standard(dut.target, 'tcp')  # Mbits/sec
    assert abs(float(match1[2]) * 8 - float(match1[3])) < 0.5
    match2 = dut.expect(r'\[\s*([12])\]\s+1.0- 2.0 sec\s+([\d\.]+) MBytes\s+([\d\.]+) Mbits/sec')
    assert abs(float(match2[3]) - float(match1[3])) < 1
    assert {int(match1[1]), int(match2[1])} == {1, 2}  # ID should be 1 and 2
    # NOTE: maybe "0.0- 8.0" or "0.0- 9.0"
    # TODO: add result to xunit report
    dut.expect(r'\[\s*[12]\]\s+0.0- [89].0 sec\s+([\d\.]+) MBytes\s+([\d\.]+) Mbits/sec')
    dut.expect(r'\[\s*[12]\]\s+0.0- [89].0 sec\s+([\d\.]+) MBytes\s+([\d\.]+) Mbits/sec')

    # udp
    time.sleep(1)
    dut.write('iperf -u -s -i 1 -t 9 --id=2')
    dut.write('iperf -u -c 127.0.0.1 -i 1 -t 9 --id=3')
    match1 = dut.expect(r'\[\s*([23])\]\s+1.0- 2.0 sec\s+([\d\.]+) MBytes\s+([\d\.]+) Mbits/sec')
    assert float(match1[2]) > _bw_standard(dut.target, 'udp') / 8
    assert float(match1[3]) > _bw_standard(dut.target, 'udp')
    assert abs(float(match1[2]) * 8 - float(match1[3])) < 0.5
    match2 = dut.expect(r'\[\s*([23])\]\s+1.0- 2.0 sec\s+([\d\.]+) MBytes\s+([\d\.]+) Mbits/sec')
    assert abs(float(match2[3]) - float(match1[3])) < 1
    dut.expect(r'\[\s*[23]\]\s+0.0- 9.0 sec\s+([\d\.]+) MBytes\s+([\d\.]+) Mbits/sec')
    dut.expect(r'\[\s*[23]\]\s+0.0- 9.0 sec\s+([\d\.]+) MBytes\s+([\d\.]+) Mbits/sec')

    # ipv6
    time.sleep(1)
    dut.write('iperf -V -s -i 1 -t 9 --id=1')
    dut.write('iperf -V -c ::1 -i 1 -t 9 --id=2')
    match1 = dut.expect(r'\[\s*([12])\]\s+1.0- 2.0 sec\s+([\d\.]+) MBytes\s+([\d\.]+) Mbits/sec')
    assert float(match1[2]) > _bw_standard(dut.target, 'tcpv6') / 8
    assert float(match1[3]) > _bw_standard(dut.target, 'tcpv6')
    assert abs(float(match1[2]) * 8 - float(match1[3])) < 0.5
    match2 = dut.expect(r'\[\s*([12])\]\s+1.0- 2.0 sec\s+([\d\.]+) MBytes\s+([\d\.]+) Mbits/sec')
    assert abs(float(match2[3]) - float(match1[3])) < 1
    dut.expect(r'\[\s*[12]\]\s+0.0- [89].0 sec\s+([\d\.]+) MBytes\s+([\d\.]+) Mbits/sec')
    dut.expect(r'\[\s*[12]\]\s+0.0- [89].0 sec\s+([\d\.]+) MBytes\s+([\d\.]+) Mbits/sec')
