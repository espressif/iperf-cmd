#!/usr/bin/env python
# SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""
Update CHANGELOG file
"""

import os
import pathlib
import re
import subprocess

from typing import Dict
from typing import List

RELEASE_TAG_BASE_URL = 'https://github.com/espressif/iperf-cmd/releases/tag'
GITHUB_COMMIT_BASE_URL = 'https://github.com/espressif/iperf-cmd/commit'
IPERF_COMPONENT_URL = 'https://components.espressif.com/components/espressif/iperf'
PROJECT_ROOT = pathlib.Path(__file__).resolve().parents[1]

CHANGELOG_SECTIONS = {
    'feat': 'Features',
    'fix': 'Bug Fixes',
    'breaking': 'Breaking Changes',
    'BREAKING CHANGE': 'Breaking Changes',
    'update': 'Updates',
    'change': 'Updates',
    'remove': 'Updates',
    'refactor': 'Updates',
    'revert': 'Updates',
}
CHANGELOG_TITLES = ['Features', 'Bug Fixes', 'Updates', 'Breaking Changes']
assert all(v in CHANGELOG_TITLES for v in CHANGELOG_SECTIONS.values())
CHANGELOG_PATTERN = re.compile(rf'({"|".join(CHANGELOG_SECTIONS.keys())})(\([^\)]+\))?:\s*([^\n]+)')
COMMIT_PATTERN = re.compile(r'^[0-9a-f]{8}')


def check_repo(old_tag: str) -> None:
    """Check current ref tag in repository"""
    subprocess.check_call(
        ['git', 'fetch', '--prune', '--prune-tags', '--force'],
        cwd=PROJECT_ROOT,
    )

    # Check old_ref in repository
    ref_tags = subprocess.check_output(
        ['git', 'show-ref', '--tags'],
        cwd=PROJECT_ROOT,
    ).decode()
    assert old_tag in ref_tags


def iter_commit_messages(git_logs: str) -> List[tuple[str, str]]:
    """Yield ``(short_hash, message)`` from ``git log --format=%H%n%B%x1e`` output.

    A record separator is required. Splitting on the word "commit" also cuts
    messages that mention pre-commit.
    """
    commits: List[tuple[str, str]] = []
    for record in git_logs.split('\x1e'):
        text = record.strip()
        if not text:
            continue
        header, sep, message = text.partition('\n')
        assert sep and COMMIT_PATTERN.match(header)
        commits.append((header[:8], message))
    return commits


def update_changelog(old_tag: str, new_tag: str) -> None:
    """Update Changelog files in iperf/iperf-cmd from git history"""
    # Update ChangeLog
    for component in ['iperf', 'iperf-cmd']:
        git_logs = subprocess.check_output(
            # ignore merge commits; separate records so messages can contain "commit"
            [
                'git',
                'log',
                '--no-merges',
                '--format=%H%n%B%x1e',
                f'{old_tag}..HEAD',
                component,
            ],
            cwd=PROJECT_ROOT,
        ).decode()

        changelogs: Dict[str, List[str]] = {k: [] for k in CHANGELOG_TITLES[::-1]}
        # Get possible changelogs from title and notes.
        for commit, message in iter_commit_messages(git_logs):
            for match in CHANGELOG_PATTERN.finditer(message):
                if match.group(2):
                    _changelog = f'- {match.group(2)}: {match.group(3)} ([{commit}]({GITHUB_COMMIT_BASE_URL}/{commit}))'
                else:
                    _changelog = f'- {match.group(3)} ([{commit}]({GITHUB_COMMIT_BASE_URL}/{commit}))'
                changelogs[CHANGELOG_SECTIONS[match.group(1)]].append(_changelog)

        if component == 'iperf-cmd':
            _changelog = f'- update dependencies [espressif/iperf]({IPERF_COMPONENT_URL}) to {new_tag}'
            changelogs['Updates'].append(_changelog)

        # Update changelog file
        with open(str(PROJECT_ROOT / component / 'CHANGELOG.md'), encoding='utf-8') as fr:
            changelog_data = fr.readlines()
        changed = False
        for key, values in changelogs.items():
            if not values:
                continue
            changelog_data.insert(2, f'### {key}\n\n' + '\n'.join(values) + '\n\n')
            changed = True
        if changed:
            changelog_data.insert(2, f'## [{new_tag}]({RELEASE_TAG_BASE_URL}/{new_tag})\n\n')
        with open(str(PROJECT_ROOT / component / 'CHANGELOG.md'), 'w', encoding='utf-8') as fw:
            fw.write(''.join(changelog_data))


if __name__ == '__main__':
    old_tag = os.environ['CZ_PRE_CURRENT_TAG_VERSION']
    new_tag = os.environ['CZ_PRE_NEW_TAG_VERSION']
    check_repo(old_tag)
    update_changelog(old_tag, new_tag)
