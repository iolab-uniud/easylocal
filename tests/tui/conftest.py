"""End-to-end tests of the TextUI: the tutorial's tester
(examples/tutorial/tui_main.cpp), the launcher of the TSP example
(examples/tsp/tui_main.cpp) and the assignment example's slow runner
(examples/assignment/tui_main.cpp).

CTest runs them as `easylocal.tui-e2e` when the TUI component and uv are
available; by hand:

    EASYLOCAL_TUTORIAL_TUI=build/<preset>/examples/tutorial/easylocal_tutorial_tui \
    EASYLOCAL_TSP_TUI=build/<preset>/examples/tsp/easylocal_tsp_tui \
    EASYLOCAL_ASSIGNMENT_TUI=build/<preset>/examples/assignment/easylocal_assignment_tui \
        uv run pytest tests/tui
"""

import os
import pathlib
import sys

import pytest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / "scripts"))

from tui_driver import Tui  # noqa: E402


@pytest.fixture
def binary() -> str:
    path = os.environ.get("EASYLOCAL_TUTORIAL_TUI")
    if not path:
        pytest.skip("EASYLOCAL_TUTORIAL_TUI is not set")
    return path


@pytest.fixture
def tui(binary):
    with Tui(binary) as driver:
        yield driver
    # A killed tester writes no coverage data: each flow must end quittable.
    assert driver.exit_status == 0, "the tester did not quit with q"


@pytest.fixture
def launcher() -> str:
    path = os.environ.get("EASYLOCAL_TSP_TUI")
    if not path:
        pytest.skip("EASYLOCAL_TSP_TUI is not set")
    return path


@pytest.fixture
def slow_runner() -> str:
    path = os.environ.get("EASYLOCAL_ASSIGNMENT_TUI")
    if not path:
        pytest.skip("EASYLOCAL_ASSIGNMENT_TUI is not set")
    return path
