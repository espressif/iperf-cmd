# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Tests for api.md stabilization after doxygen."""

import pathlib
import tempfile
import unittest

from doxygen_api_md import stabilize_api_md


class StabilizeApiMdTest(unittest.TestCase):
    def test_drops_anonymous_union_placeholder(self) -> None:
        original = 'Variables:\n' '\n' '-  anonymous union @1  \n' '\n' '-  connect_info\n'
        expected = 'Variables:\n\n-  connect_info\n'
        with tempfile.TemporaryDirectory() as tmp:
            api_md = pathlib.Path(tmp) / 'api.md'
            api_md.write_text(original)
            stabilize_api_md(api_md)
            self.assertEqual(api_md.read_text(), expected)

    def test_leaves_stable_api_md_unchanged(self) -> None:
        original = 'Variables:\n\n-  connect_info\n'
        with tempfile.TemporaryDirectory() as tmp:
            api_md = pathlib.Path(tmp) / 'api.md'
            api_md.write_text(original)
            stabilize_api_md(api_md)
            self.assertEqual(api_md.read_text(), original)


if __name__ == '__main__':
    unittest.main()
