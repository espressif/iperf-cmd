# SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Check all versions from idf_component.yml
"""
import argparse
import os
import pathlib

import yaml

PROJ_ROOT_PATH = pathlib.Path(__file__).parents[1]
COMPONENT_YML_FILES = PROJ_ROOT_PATH.glob('**/idf_component.yml')
TAG_VERSION = os.environ.get('CI_COMMIT_TAG')


def check_version(version: str):
    """Check iperf and iperf-cmd versions"""
    for component_yml in COMPONENT_YML_FILES:
        with open(component_yml, 'r', encoding='utf-8') as f:
            data = yaml.load(f, Loader=yaml.SafeLoader)
        assert data
        # check component version
        if 'version' in data:
            assert version == data['version'], f'Version mismatch: {component_yml}'
        # check dependencies
        if 'dependencies' not in data:
            continue
        dependencies = data['dependencies']
        for key, val in dependencies.items():
            if key in ['espressif/iperf', 'espressif/iperf-cmd']:
                assert (
                    version == val['version']
                ), f'dependencies version mismatch: {component_yml}'


def main():
    """main"""
    parser = argparse.ArgumentParser()
    parser.add_argument('--version', type=str, default=TAG_VERSION)
    args = parser.parse_args()
    check_version(args.version)


if __name__ == '__main__':
    main()
