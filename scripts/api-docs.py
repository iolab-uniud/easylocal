#!/usr/bin/env python3
"""Generate the API reference with MrDocs, into build/site-api.

MrDocs reads the headers through a compilation database. The script writes one
with a single translation unit that includes every public header: an adapter
(TOML, TUI, REST) only when its dependency is on the include path. The flags
are the include paths and definitions of the compilation database of a
configured build, so that the adapters' dependencies are found; configure it
with every optional component for a complete reference:

    cmake --preset dev -B build/api -DEASYLOCAL_ENABLE_CONFIG_TOML=ON \\
        -DEASYLOCAL_ENABLE_TUI=ON -DEASYLOCAL_ENABLE_REST=ON \\
        -DEASYLOCAL_FETCH_DEPENDENCIES=ON
    uv run scripts/api-docs.py build/api

The MrDocs executable is $MRDOCS, or mrdocs on the PATH. Standard library
only: `uv run scripts/api-docs.py` or `python3`.
"""

import argparse
import json
import os
import pathlib
import shlex
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
INCLUDE = ROOT / "include"
# adapter header -> the dependency header it needs
ADAPTERS = {
    "easylocal/adapters/toml.hpp": "toml++/toml.hpp",
    "easylocal/adapters/tui.hpp": "ftxui/ftxui.hpp",
    "easylocal/adapters/rest.hpp": "crow.h",
}


def public_headers():
    """The headers of include/easylocal, outside detail/ and the adapters."""
    for path in sorted((INCLUDE / "easylocal").rglob("*.hpp")):
        relative = path.relative_to(INCLUDE).as_posix()
        if "/detail/" in relative or relative.startswith("easylocal/adapters"):
            continue
        yield relative


def translation_unit():
    lines = [f"#include <{header}>" for header in public_headers()]
    for adapter, dependency in ADAPTERS.items():
        lines += [f"#if __has_include(<{dependency}>)", f"#include <{adapter}>", "#endif"]
    return "\n".join(lines) + "\n"


def build_flags(database):
    """The include paths and definitions of every entry, once each."""
    flags = []
    for entry in json.loads(database.read_text()):
        arguments = entry.get("arguments") or shlex.split(entry["command"])
        i = 1
        while i < len(arguments):
            argument = arguments[i]
            if argument in ("-I", "-isystem") and i + 1 < len(arguments):
                flag = [argument, arguments[i + 1]]
                i += 2
            elif argument.startswith(("-I", "-D", "-isystem")):
                flag = [argument]
                i += 1
            else:
                i += 1
                continue
            if flag not in flags:
                flags.append(flag)
    return [argument for flag in flags for argument in flag]


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("build", type=pathlib.Path, help="a configured build directory")
    parser.add_argument(
        "--output", type=pathlib.Path, default=ROOT / "build" / "site-api",
        help="the directory of the generated pages (default: build/site-api)")
    args = parser.parse_args()

    database = args.build.resolve() / "compile_commands.json"
    if not database.is_file():
        sys.exit(f"{database} not found: configure the build first")
    mrdocs = os.environ.get("MRDOCS") or shutil.which("mrdocs")
    if not mrdocs:
        sys.exit("mrdocs not found: set MRDOCS or put it on the PATH")

    work = args.build.resolve() / "api-docs"
    work.mkdir(parents=True, exist_ok=True)
    source = work / "easylocal_api.cpp"
    source.write_text(translation_unit())
    arguments = ["clang++", "-std=c++23", *build_flags(database), "-c", str(source)]
    (work / "compile_commands.json").write_text(json.dumps(
        [{"directory": str(work), "file": str(source), "arguments": arguments}], indent=1))

    output = args.output.resolve()
    shutil.rmtree(output, ignore_errors=True)
    return subprocess.call([
        mrdocs,
        f"--config={ROOT / 'docs' / 'mrdocs.yml'}",
        f"--compilation-database={work / 'compile_commands.json'}",
        f"--output={output}",
    ])


if __name__ == "__main__":
    sys.exit(main())
