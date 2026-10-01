"""Drive an EasyLocal TextUI in a pseudo-terminal, for tests and screenshots.

The binary runs in a pty of fixed size; its output is fed to a VT100 emulator
(pyte), so `screen()` is what a user would see. Interactions wait for the
screen instead of sleeping, in the spirit of Playwright's auto-waiting:

    with Tui(binary) as tui:
        tui.press("I")
        tui.expect("COST 29")
        tui.press(F5)
        tui.select("sa")
        tui.press("G")
        tui.expect("Runner completed: sa", timeout=30)
"""

from __future__ import annotations

import fcntl
import os
import pty
import re
import select
import signal
import struct
import termios
import time
from typing import Callable, Pattern

import pyte

F1, F2, F3, F4 = "\x1bOP", "\x1bOQ", "\x1bOR", "\x1bOS"
F5 = "\x1b[15~"
UP, DOWN, RIGHT, LEFT = "\x1b[A", "\x1b[B", "\x1b[C", "\x1b[D"
ENTER, ESCAPE, TAB, BACKSPACE = "\r", "\x1b", "\t", "\x7f"
HOME, END, DELETE = "\x1b[H", "\x1b[F", "\x1b[3~"


class TuiError(AssertionError):
    """An expectation on the screen failed; the message carries the screen."""


class Tui:
    def __init__(self, binary: str, columns: int = 100, lines: int = 30,
                 args: tuple[str, ...] = ()) -> None:
        self.binary, self.args = binary, args
        self.columns, self.lines = columns, lines
        self.screen_buffer = pyte.Screen(columns, lines)
        self.stream = pyte.ByteStream(self.screen_buffer)
        self.pid = -1
        self.fd = -1
        self.exit_status: int | None = None

    # -- lifetime -------------------------------------------------------------

    def start(self) -> "Tui":
        pid, fd = pty.fork()
        if pid == 0:
            os.environ["TERM"] = "xterm-256color"
            try:
                os.execv(self.binary, [self.binary, *self.args])
            finally:
                os._exit(127)
        self.pid, self.fd = pid, fd
        fcntl.ioctl(fd, termios.TIOCSWINSZ,
                    struct.pack("HHHH", self.lines, self.columns, 0, 0))
        self.settle(quiet=1.0)
        return self

    def __enter__(self) -> "Tui":
        return self.start()

    def __exit__(self, *exc) -> None:
        self.kill()

    def kill(self) -> None:
        if self.pid > 0 and self.exit_status is None:
            try:
                os.kill(self.pid, signal.SIGKILL)
                os.waitpid(self.pid, 0)
            except (ProcessLookupError, ChildProcessError):
                pass
        if self.fd >= 0:
            os.close(self.fd)
            self.fd = -1

    def wait_exit(self, timeout: float = 5.0) -> int:
        """Wait for the program to end and return its exit status."""
        deadline = time.time() + timeout
        while time.time() < deadline:
            self.pump(0.05)
            pid, status = os.waitpid(self.pid, os.WNOHANG)
            if pid == self.pid:
                self.exit_status = os.waitstatus_to_exitcode(status)
                return self.exit_status
        raise TuiError(f"the program did not exit within {timeout}s\n{self.text()}")

    # -- output ---------------------------------------------------------------

    def pump(self, wait: float) -> bool:
        """Feed the output available within `wait` seconds; False on EOF."""
        ready, _, _ = select.select([self.fd], [], [], wait)
        if not ready:
            return True
        try:
            data = os.read(self.fd, 65536)
        except OSError:
            return False
        if not data:
            return False
        self.stream.feed(data)
        return True

    def settle(self, quiet: float = 0.15, limit: float = 8.0) -> None:
        """Feed output until the screen has been quiet for `quiet` seconds."""
        start = last = time.time()
        while time.time() - start < limit:
            ready, _, _ = select.select([self.fd], [], [], 0.05)
            if ready:
                if not self.pump(0):
                    return
                last = time.time()
            elif time.time() - last > quiet:
                return

    def text(self) -> str:
        return "\n".join(line.rstrip() for line in self.screen_buffer.display)

    screen = text

    # -- input ----------------------------------------------------------------

    def press(self, *keys: str) -> "Tui":
        for key in keys:
            os.write(self.fd, key.encode())
            self.settle()
        return self

    def type(self, text: str) -> "Tui":
        return self.press(*text)

    # -- expectations ---------------------------------------------------------

    def wait_until(self, condition: Callable[[str], bool], timeout: float = 10.0,
                   what: str = "condition") -> str:
        deadline = time.time() + timeout
        while True:
            screen = self.text()
            if condition(screen):
                return screen
            if time.time() > deadline:
                raise TuiError(f"timed out after {timeout}s waiting for {what}\n{screen}")
            self.pump(0.1)

    def expect(self, pattern: str | Pattern[str], timeout: float = 10.0) -> re.Match[str] | None:
        """Wait until the screen shows `pattern` (text, or a compiled regex)."""
        if isinstance(pattern, str):
            self.wait_until(lambda s: pattern in s, timeout, repr(pattern))
            return None
        screen = self.wait_until(lambda s: pattern.search(s) is not None, timeout,
                                 f"/{pattern.pattern}/")
        return pattern.search(screen)

    def expect_absent(self, text: str) -> None:
        if text in self.text():
            raise TuiError(f"unexpected {text!r} on screen\n{self.text()}")

    def select(self, item: str, key: str = DOWN, attempts: int = 20) -> "Tui":
        """Press `key` until the list cursor `> item` is on screen."""
        marker = f"> {item}"
        for _ in range(attempts):
            if marker in self.text():
                return self
            self.press(key)
        raise TuiError(f"{marker!r} never appeared\n{self.text()}")

    def cost(self) -> int | float:
        """The current cost shown in the header (`COST n`)."""
        match = re.search(r"COST (-?\d+(?:\.\d+)?)", self.text())
        if match is None:
            raise TuiError(f"no COST on screen\n{self.text()}")
        value = match.group(1)
        return float(value) if "." in value else int(value)
