# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Tests for changelog commit parsing."""

import unittest

from changelog import iter_commit_messages


class IterCommitMessagesTest(unittest.TestCase):
    def test_keeps_message_that_mentions_pre_commit(self) -> None:
        raw = (
            '0249b1967771b3b6de6ef7d9c0fa4aa174fea1a3\n'
            'fix: ignore placeholder\n'
            '\n'
            'The pre-commit hook rewrites the file.\n'
            '\x1e'
            'add6b71534eebb73d24795475a8cfae8e3e528be\n'
            'feat: make timeout configurable\n'
            '\x1e'
        )
        self.assertEqual(
            iter_commit_messages(raw),
            [
                ('0249b196', 'fix: ignore placeholder\n\nThe pre-commit hook rewrites the file.'),
                ('add6b715', 'feat: make timeout configurable'),
            ],
        )


if __name__ == '__main__':
    unittest.main()
