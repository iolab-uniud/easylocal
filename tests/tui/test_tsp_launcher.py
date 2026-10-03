"""The launcher of the TSP example: two apps on one instance, each opened in
the tester and left with q, back to the list, sharing the Input and the
current solution."""

import re

from tui_driver import DOWN, ENTER, ESCAPE, F3, F4, UP, Tui


def launcher_shown(screen: str) -> bool:
    """The list is the last thing drawn: the launcher renders inline, below
    whatever the tester left on the terminal."""
    lines = [line for line in screen.splitlines() if line.strip()]
    return len(lines) >= 2 and "q/Esc: quit" in lines[-2]


def open_app(tui: Tui, name: str, key: str) -> None:
    tui.wait_until(launcher_shown, what="the list of applications")
    tui.select(name, key)
    tui.press(ENTER)
    tui.expect(f"EasyLocal TSP Tester - {name}")


def test_each_app_opens_in_the_tester_and_q_goes_back(launcher):
    with Tui(launcher) as tui:
        open_app(tui, "tsp-swap", DOWN)
        tui.expect("small.tsp")
        tui.expect("q back to")  # the exit label of a launched tester
        tui.press("I")
        tui.expect("Initial solution selected")
        tui.press(F3, "C")
        tui.expect(re.compile(r"Check passed: \d+ semantic checks"))
        tui.press("q")

        open_app(tui, "tsp-two-opt", UP)
        tui.press("q")

        tui.wait_until(launcher_shown, what="the list of applications")
        tui.press("q")
        assert tui.wait_exit() == 0


def test_the_apps_share_the_input_and_the_solution(launcher):
    with Tui(launcher) as tui:
        tui.wait_until(launcher_shown, what="the list of applications")
        tui.expect("Input shared by the applications; no solution yet")

        # A solution made with the swap neighborhood...
        open_app(tui, "tsp-swap", DOWN)
        tui.press("I")
        tui.expect("Initial solution selected")
        tui.press("q")
        tui.wait_until(launcher_shown, what="the list of applications")
        tui.expect("Input and solution shared by the applications")

        # ...is the current solution of the 2-opt app: its cost is in the
        # header as soon as the tester opens.
        open_app(tui, "tsp-two-opt", UP)
        tui.expect(re.compile(r"COST \d+"))
        tui.press("q")

        tui.wait_until(launcher_shown, what="the list of applications")
        tui.press("q")
        assert tui.wait_exit() == 0


def test_the_root_owns_the_input_and_the_solution(launcher):
    with Tui(launcher) as tui:
        # The root: the Input/Output page only, with the loading commands.
        open_app(tui, "Input and solution", UP)
        tui.expect("Input and solution for all the applications")
        tui.expect("L Load input")
        tui.press("I")
        tui.expect("Initial solution selected")
        tui.press(F4)
        tui.expect("Move and Run are in the applications")
        tui.press("q")

        # A child opens on the root's solution and loads no files.
        open_app(tui, "tsp-two-opt", DOWN)
        tui.expect(re.compile(r"COST \d+"))
        tui.press(F3)
        tui.expect("Input and solution shared with the launcher's applications")
        tui.expect("I Initial")
        tui.expect_absent("L Load input")
        tui.expect_absent("Shift-L Load")
        tui.press("q")

        tui.wait_until(launcher_shown, what="the list of applications")
        tui.press("q")
        assert tui.wait_exit() == 0


def test_escape_quits_the_launcher(launcher):
    with Tui(launcher) as tui:
        tui.wait_until(launcher_shown, what="the list of applications")
        tui.press(ESCAPE)
        assert tui.wait_exit() == 0
