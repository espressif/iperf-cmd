#!/usr/bin/env python
# SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
'''
Update CHANGELOG file
'''
import os
import pathlib
import re
import subprocess


RELEASE_TAG_BASE_URL = 'https://github.com/espressif/iperf-cmd/releases/tag'
COMMIT_BASE_URL = 'https://github.com/espressif/iperf-cmd/commit'
IPERF_COMPONENT_URL = 'https://components.espressif.com/components/espressif/iperf'
PROJECT_ROOT = pathlib.Path(__file__).resolve().parents[1]
CZ_OLD_TAG = os.environ['CZ_PRE_CURRENT_TAG_VERSION']
CZ_NEW_TAG = os.environ['CZ_PRE_NEW_TAG_VERSION']

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
assert all((v in CHANGELOG_TITLES for v in CHANGELOG_SECTIONS.values()))
CHANGELOG_PATTERN = re.compile(rf'({"|".join(CHANGELOG_SECTIONS.keys())})(\([^\)]+\))?:\s*([^\n]+)')
COMMIT_PATTERN = re.compile(r'^[0-9a-f]{8}')


def check_repo():
    '''Check current ref tag in repository
    '''
    subprocess.check_call(
        ['git', 'fetch', '--prune', '--prune-tags', '--force'],
        cwd=PROJECT_ROOT,
        )

    # Check old_ref in repository
    ref_tags = subprocess.check_output(
        ['git', 'show-ref', '--tags'],
        cwd=PROJECT_ROOT,
    ).decode()
    assert CZ_OLD_TAG in ref_tags


def update_changelog():
    """Update Changelog files in iperf/iperf-cmd from git history
    """
    # Update ChangeLog
    for component in ['iperf', 'iperf-cmd']:
        git_logs = subprocess.check_output(
            # ignore merge commits
            ['git', 'log', '--no-merges', f'{CZ_OLD_TAG}..HEAD', f'{component}'],
            cwd=PROJECT_ROOT,
        ).decode()

        changelogs = {k: [] for k in CHANGELOG_TITLES[::-1]}
        # Get possible changelogs from title and notes.
        for commit_log in git_logs.split('commit ')[1:]:
            commit = COMMIT_PATTERN.match(commit_log).group(0)
            for match in CHANGELOG_PATTERN.finditer(commit_log):
                if match.group(2):
                    _changelog = f'- {match.group(2)}: {match.group(3)} ([{commit}]({COMMIT_BASE_URL}/{commit}))'
                else:
                    _changelog = f'- {match.group(3)} ([{commit}]({COMMIT_BASE_URL}/{commit}))'
                changelogs[CHANGELOG_SECTIONS[match.group(1)]].append(_changelog)

        if component == 'iperf-cmd':
            _changelog = f'- update dependencies [espressif/iperf]({IPERF_COMPONENT_URL}) to {CZ_NEW_TAG}'
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
            changelog_data.insert(2, f'## [{CZ_NEW_TAG}]({RELEASE_TAG_BASE_URL}/{CZ_NEW_TAG})\n\n')
        with open(str(PROJECT_ROOT / component / 'CHANGELOG.md'), 'w', encoding='utf-8') as fw:
            fw.write(''.join(changelog_data))


if __name__ == '__main__':
    check_repo()
    update_changelog()
