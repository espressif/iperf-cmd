#!/usr/bin/env python
#
# SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Run doxybook to update api.md"""

import pathlib
import re
import subprocess

PROJ_ROOT_PATH = pathlib.Path(__file__).parents[1]

# Older Doxygen names an anonymous union "@1" and esp-doxybook keeps that
# placeholder. Newer Doxygen leaves the name empty and current esp-doxybook
# drops it. Strip the placeholder so api.md does not depend on that version.
_ANON_UNION_LINE = re.compile(r'\n-  anonymous union @\d+[ \t]*\n\n')


def stabilize_api_md(api_md: pathlib.Path) -> None:
    """Drop the version-specific anonymous-union placeholder from api.md."""
    text = api_md.read_text()
    stabilized = _ANON_UNION_LINE.sub('\n', text)
    if stabilized != text:
        api_md.write_text(stabilized)


def main() -> None:
    """run esp-doxybook to update api.md"""
    for component in ['iperf', 'iperf-cmd']:
        component_dir = PROJ_ROOT_PATH / component
        subprocess.check_call(['doxygen', 'Doxyfile'], cwd=component_dir)
        subprocess.check_call(['esp-doxybook', '-i', 'doxygen/xml', '-o', './api.md'], cwd=component_dir)
        stabilize_api_md(component_dir / 'api.md')


if __name__ == '__main__':
    main()
