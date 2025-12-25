# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
import difflib
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile

import gitlab
import yaml

PROJECT_ROOT = pathlib.Path(__file__).resolve().parents[2]
# add notes to MR
GITLAB_API_TOKEN = os.getenv('GITLAB_API_TOKEN')
CI_SERVER_URL = os.getenv('CI_SERVER_URL')
CI_PROJECT_ID = os.getenv('CI_PROJECT_ID')
CI_MERGE_REQUEST_IID = os.getenv('CI_MERGE_REQUEST_IID')


def add_or_modify_mr_notes(content: str, header: str) -> None:
    """Add or modify notes to MR"""
    gl = gitlab.Gitlab(url=CI_SERVER_URL, private_token=GITLAB_API_TOKEN)
    gl.auth()
    project = gl.projects.get(CI_PROJECT_ID, lazy=True)
    mr = project.mergerequests.get(CI_MERGE_REQUEST_IID, lazy=True)

    discussions = mr.discussions.list(all=True, iter=True, per_page=100)
    for discussion in discussions:
        for note in discussion.attributes['notes']:
            if header not in note['body']:
                continue
            # update content
            note_obj = discussion.notes.get(note['id'])
            note_obj.body = content
            note_obj.save()
            # updated
            return
    # Discussion not found, create a new one.
    note = mr.notes.create({'body': content})
    note.save()


def get_new_changelog(old_file: str, new_file: str) -> str:
    """Get new changelog"""
    with open(old_file) as f1, open(new_file) as f2:
        lines1 = f1.readlines()
        lines2 = f2.readlines()

    diff = difflib.ndiff(lines1, lines2)

    added_lines = []
    for line in diff:
        if line.startswith('+ '):
            added_lines.append(line[2:])

    return ''.join(added_lines)


def main() -> None:
    """Preview changelog"""
    # get current version
    current_version = ''
    with open(PROJECT_ROOT / 'iperf-cmd' / 'idf_component.yml') as f:
        data = yaml.safe_load(f)
        current_version = 'v' + data['version']

    current_commit_msg = subprocess.check_output(
        ['git', 'log', '-1'],
        cwd=PROJECT_ROOT,
    ).decode('utf-8')
    if 'ci(bump)' in current_commit_msg or 'bump/new_version' in current_commit_msg:
        print('Skip preview changelog for new version commit, exiting.')
        sys.exit(0)

    # copy files to new temp directory
    tmp_dir = tempfile.mkdtemp()
    tmp_repo_dir = os.path.join(tmp_dir, 'iperf-cmd')
    shutil.copytree(str(PROJECT_ROOT), tmp_repo_dir)
    subprocess.check_call(
        ['git', 'log', '-10', '--pretty=oneline'],
        cwd=tmp_repo_dir,
    )
    try:
        # bump new version
        next_version = (
            'v'
            + subprocess.check_output(
                ['cz', 'bump', '--get-next', '--yes'],
                cwd=tmp_repo_dir,
                stderr=subprocess.PIPE,
            )
            .decode('utf-8')
            .strip()
        )
        subprocess.check_call(
            ['cz', 'bump', '--devrelease', '1', '--yes'],
            cwd=tmp_repo_dir,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
    except subprocess.CalledProcessError as e:
        print(f'Error during version bump: {type(e)}: {str(e)}')
        raise e
    # get changelog
    iperf_changelog = get_new_changelog(
        str(PROJECT_ROOT / 'iperf' / 'CHANGELOG.md'),
        os.path.join(tmp_repo_dir, 'iperf', 'CHANGELOG.md'),
    )
    iperf_cmd_changelog = get_new_changelog(
        str(PROJECT_ROOT / 'iperf-cmd' / 'CHANGELOG.md'),
        os.path.join(tmp_repo_dir, 'iperf-cmd', 'CHANGELOG.md'),
    )
    # remove temp directory
    shutil.rmtree(tmp_dir)
    # show changelog or comment to MR
    # usually iperf-cmd changelog include all iperf changelog
    notes = (
        f'# Preview changelog\n'
        f'- from {current_version} to {next_version}\n'
        f'## Changelog (iperf)\n'
        f'<details><summary> Click to see more instructions ... </summary><p><br>\n'
        f'{iperf_changelog}\n'
        f'</details>\n\n'
        f'## Changelog (iperf-cmd)\n'
        # f'<details><summary> Click to see more instructions ... </summary><p><br>\n'
        f'{iperf_cmd_changelog}\n'
        # f'</details>\n'
    )
    print(notes)
    if GITLAB_API_TOKEN and CI_MERGE_REQUEST_IID:
        add_or_modify_mr_notes(notes, header='# Preview changelog')
    else:
        print('not running in GitLab CI, skipping MR notes update')


if __name__ == '__main__':
    main()
