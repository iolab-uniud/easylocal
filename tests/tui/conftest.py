"""End-to-end tests of the tutorial's TextUI tester (examples/tutorial/tui_main.cpp).

CTest runs them as `easylocal.tui-e2e` when the TUI component and uv are
available; by hand:

    EASYLOCAL_TUTORIAL_TUI=build/<preset>/examples/tutorial/easylocal_tutorial_tui \
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
