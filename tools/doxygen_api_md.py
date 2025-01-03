#!/usr/bin/env python
#
# SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Run doxybook to update api.md"""

import pathlib
import subprocess

PROJ_ROOT_PATH = pathlib.Path(__file__).parents[1]


def main() -> None:
    """run esp-doxybook to update api.md"""
    for component in ['iperf', 'iperf-cmd']:
        component_dir = str(PROJ_ROOT_PATH / component)
        subprocess.check_call(['doxygen', 'Doxyfile'], cwd=component_dir)
        subprocess.check_call(['esp-doxybook', '-i', 'doxygen/xml', '-o', './api.md'], cwd=component_dir)


if __name__ == '__main__':
    main()
