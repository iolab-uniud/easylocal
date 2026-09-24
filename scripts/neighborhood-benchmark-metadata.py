#!/usr/bin/env python3
"""Emit structured metadata for one neighborhood-authoring benchmark run."""

from __future__ import annotations

import argparse
import csv
import os
from pathlib import Path
import platform
import shlex
import shutil
import subprocess
import sys
from typing import Optional


def command_output(command: list[str], *, stdin: Optional[str] = None) -> str:
    try:
        completed = subprocess.run(
            command,
            input=stdin,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
        )
    except OSError:
        return "unavailable"
    if completed.returncode != 0:
        return "unavailable"
    return completed.stdout.strip()


def first_line(text: str) -> str:
    return text.splitlines()[0] if text else "unavailable"


def resolve_compiler() -> str:
    requested = os.environ.get("CXX", "c++")
    if os.path.sep in requested:
        return str(Path(requested).expanduser().resolve())
    return shutil.which(requested) or requested


def stdlib_metadata(compiler: str, cxxflags: list[str]) -> tuple[str, str]:
    macros = command_output(
        [compiler, *cxxflags, "-std=c++23", "-dM", "-E", "-x", "c++", "-"],
        stdin="#include <version>\n",
    )
    for line in macros.splitlines():
        if line.startswith("#define _LIBCPP_VERSION "):
            return "libc++", line.rsplit(" ", 1)[-1]
        if line.startswith("#define __GLIBCXX__ "):
            return "libstdc++", line.rsplit(" ", 1)[-1]
    return "unknown", "unknown"


def os_description() -> str:
    if platform.system() == "Darwin":
        name = command_output(["sw_vers", "-productName"])
        version = command_output(["sw_vers", "-productVersion"])
        build = command_output(["sw_vers", "-buildVersion"])
        return f"{name} {version} ({build})"
    os_release = Path("/etc/os-release")
    if os_release.exists():
        for line in os_release.read_text(encoding="utf-8").splitlines():
            if line.startswith("PRETTY_NAME="):
                return line.split("=", 1)[1].strip().strip('"')
    return platform.platform()


def hardware_model() -> str:
    if platform.system() == "Darwin":
        return command_output(["sysctl", "-n", "hw.model"])
    if platform.system() == "Linux":
        output = command_output(["lscpu"])
        for line in output.splitlines():
            if line.startswith("Model name:"):
                return line.split(":", 1)[1].strip()
    return "unknown"


def git_metadata(repo_root: Path) -> tuple[str, str]:
    commit = command_output(["git", "-C", str(repo_root), "rev-parse", "HEAD"])
    status = command_output(["git", "-C", str(repo_root), "status", "--porcelain"])
    dirty = "unknown" if status == "unavailable" else ("true" if status else "false")
    return commit, dirty


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--repo-root", required=True, type=Path)
    parser.add_argument("--target-moves", required=True)
    parser.add_argument("--trials", required=True)
    parser.add_argument("--seed", required=True)
    args = parser.parse_args()

    compiler = resolve_compiler()
    cxxflags_text = os.environ.get("CXXFLAGS", "")
    cxxflags = shlex.split(cxxflags_text)
    stdlib, stdlib_version = stdlib_metadata(compiler, cxxflags)
    git_commit, git_dirty = git_metadata(args.repo_root)

    compiler_version = first_line(command_output([compiler, "--version"]))
    compiler_target = first_line(command_output([compiler, "-dumpmachine"]))
    cmake_version = first_line(command_output(["cmake", "--version"]))
    ninja_version = first_line(command_output(["ninja", "--version"]))

    rows = [
        ("schema_version", "1"),
        ("toolchain", os.environ.get("TOOLCHAIN", "local")),
        ("os", platform.system()),
        ("os_release", platform.release()),
        ("os_description", os_description()),
        ("architecture", platform.machine()),
        ("runner_os", os.environ.get("RUNNER_OS", "local")),
        ("runner_arch", os.environ.get("RUNNER_ARCH", "local")),
        ("hardware_model", hardware_model()),
        ("logical_cpus", str(os.cpu_count() or "unknown")),
        ("compiler", compiler),
        ("compiler_version", compiler_version),
        ("compiler_target", compiler_target),
        ("cxxflags", cxxflags_text),
        ("stdlib", stdlib),
        ("stdlib_version", stdlib_version),
        ("cmake", cmake_version),
        ("ninja", ninja_version),
        ("sdkroot", os.environ.get("SDKROOT", "")),
        (
            "xcode",
            first_line(command_output(["xcodebuild", "-version"]))
            if platform.system() == "Darwin"
            else "n/a",
        ),
        ("generator", "Ninja"),
        ("build_type", "Release"),
        ("cpp_standard", "23"),
        ("target_moves", args.target_moves),
        ("runner_target_evaluations", args.target_moves),
        ("trials", args.trials),
        ("seed", args.seed),
        ("git_commit", git_commit),
        ("git_dirty", git_dirty),
    ]

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle, lineterminator="\n")
        writer.writerow(["key", "value"])
        writer.writerows(rows)
    return 0


if __name__ == "__main__":
    sys.exit(main())
