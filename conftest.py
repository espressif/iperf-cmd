# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
import logging
import os
import pathlib
import sys

from datetime import datetime
from typing import Callable
from typing import List
from typing import Optional
from typing import Tuple

import pytest

from pytest import Config
from pytest import FixtureRequest
from pytest import Session
from pytest_embedded.plugin import multi_dut_argument
from pytest_embedded.plugin import multi_dut_fixture

PYTEST_ROOT_DIR = str(pathlib.Path(__file__).parent)
logging.info(f'Pytest root dir: {PYTEST_ROOT_DIR}')


@pytest.fixture(scope='session', autouse=True)
def session_tempdir() -> str:
    _tmpdir = os.path.join(
        os.path.dirname(__file__),
        'pytest_log',
        datetime.now().strftime('%Y-%m-%d_%H-%M-%S'),
    )
    os.makedirs(_tmpdir, exist_ok=True)
    return _tmpdir


@pytest.fixture
@multi_dut_argument
def config(request: FixtureRequest) -> str:
    config_marker = list(request.node.iter_markers(name='config'))
    if config_marker:
        assert isinstance(config_marker[0].args[0], str)
        return config_marker[0].args[0]
    return 'default'


@pytest.fixture
@multi_dut_argument
def app_path(request: FixtureRequest, test_file_path: str) -> str:
    config_marker = list(request.node.iter_markers(name='app_path'))
    if config_marker:
        assert isinstance(config_marker[0].args[0], str)
        return config_marker[0].args[0]
    # compatible with old pytest-embedded parametrize --app_path
    return request.config.getoption('app_path', None) or os.path.dirname(test_file_path)


@pytest.fixture
def test_case_name(request: FixtureRequest, target: str, config: str) -> str:
    if not isinstance(target, str):
        target = '|'.join(sorted(set(target)))
    if not isinstance(config, str):
        config = '|'.join(sorted(config))
    return f'{target}.{config}.{request.node.originalname}'


@pytest.fixture
@multi_dut_fixture
def build_dir(app_path: str, target: Optional[str], config: Optional[str]) -> Optional[str]:
    """
    Check local build dir with the following priority:

    1. <app_path>/build_<target>_<config>
    2. <app_path>/build
    3. <app_path>

    Args:
        app_path: app path
        target: target
        config: config

    Returns:
        valid build directory
    """

    assert target
    assert config
    check_dirs = []
    check_dirs.append(os.path.join(app_path, f'build_{target}_{config}'))
    check_dirs.append(os.path.join(app_path, 'build'))
    check_dirs.append(app_path)

    for binary_path in check_dirs:
        if os.path.isdir(binary_path):
            logging.info(f'find valid binary path: {binary_path}')
            return binary_path

        logging.warning(f'checking binary path: {binary_path} ... missing ... try another place')

    logging.error(f'no build dir. Please build the binary "python tools/build_apps.py {app_path}" and run pytest again')
    sys.exit(1)


@pytest.fixture(autouse=True)
@multi_dut_fixture
def junit_properties(test_case_name: str, record_xml_attribute: Callable[[str, object], None]) -> None:
    """
    This fixture is autoused and will modify the junit report test case name to <target>.<config>.<case_name>
    """
    record_xml_attribute('name', test_case_name)


##################
# Hook functions #
##################
_idf_pytest_embedded_key = pytest.StashKey['IdfPytestEmbedded']


def pytest_addoption(parser: pytest.Parser) -> None:
    base_group = parser.getgroup('idf')
    base_group.addoption(
        '--env',
        help='only run tests matching the environment NAME.',
    )


def pytest_configure(config: Config) -> None:
    # Require cli option "--target"
    help_commands = ['--help', '--fixtures', '--markers', '--version', '--collect-only']
    for cmd in help_commands:
        if cmd in config.invocation_params.args:
            target = 'not-needed'
            break
    else:
        target = config.getoption('target')
    if not target:
        raise ValueError('Please specify one target marker via "--target [TARGET]"')

    config.stash[_idf_pytest_embedded_key] = IdfPytestEmbedded(  # type: ignore
        target=target,
        env_name=config.getoption('env'),
    )
    config.pluginmanager.register(config.stash[_idf_pytest_embedded_key])  # type: ignore


def pytest_unconfigure(config: Config) -> None:
    _pytest_embedded = config.stash.get(_idf_pytest_embedded_key, None)  # type: ignore
    if _pytest_embedded:
        del config.stash[_idf_pytest_embedded_key]
        config.pluginmanager.unregister(_pytest_embedded)


class IdfPytestEmbedded:
    def __init__(
        self,
        target: Optional[str] = None,
        env_name: Optional[str] = None,
    ):
        # CLI options to filter the test cases
        self.target = target
        self.env_name = env_name

        self._failed_cases: List[Tuple[str, bool, bool]] = []  # (test_case_name, is_known_failure_cases, is_xfail)

    @pytest.hookimpl(tryfirst=True)
    def pytest_sessionstart(self, session: Session) -> None:
        if self.target:
            self.target = self.target.lower()
            session.config.option.target = self.target

    # @pytest.hookimpl(tryfirst=True)
    def pytest_collection_modifyitems(self, items: List[pytest.Function]) -> None:
        def item_targets(item: pytest.Function) -> List[str]:
            return [m.args[0] for m in item.iter_markers(name='target')]

        def item_envs(item: pytest.Function) -> List[str]:
            return [m.args[0] for m in item.iter_markers(name='env')]

        # set default timeout 10 minutes for each case
        for item in items:
            # default timeout 5 mins
            if 'timeout' not in item.keywords:
                item.add_marker(pytest.mark.timeout(5 * 60))

        # filter all the test cases with "--target"
        if self.target:
            items[:] = [item for item in items if self.target in item_targets(item)]

        # filter all the test cases with "--env"
        if self.env_name:
            items[:] = [item for item in items if self.env_name in item_envs(item)]
