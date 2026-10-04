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

With --undocumented it writes no pages but lists the public declarations
without a comment, as file:line, kind and name, optionally only those of the
headers under --only (a path relative to include/easylocal, such as trace/):

    uv run scripts/api-docs.py build/api --undocumented --only trace/

The MrDocs executable is $MRDOCS, or mrdocs on the PATH. Standard library
only: `uv run scripts/api-docs.py` or `python3`.
"""

import argparse
import json
import os
import pathlib
import re
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


UNDOCUMENTED = re.compile(
    r"^(?P<path>/\S+?\.hpp):(?P<line>\d+):\d+:\s*\n\s*1\) (?P<name>.+?): "
    r"(?:(?P<kind>\w+) is undocumented|Missing documentation for (?P<what>enum value))",
    re.M)


def undocumented(log, only):
    """The declarations without a comment that MrDocs reported in log."""
    log = re.sub(r"\x1b\[[0-9;]*m", "", log)
    prefix = (INCLUDE / "easylocal").as_posix() + "/"
    found = set()
    for match in UNDOCUMENTED.finditer(log):
        path = match["path"]
        if not path.startswith(prefix):
            continue
        relative = path[len(prefix):]
        if only and not relative.startswith(only):
            continue
        found.add((relative, int(match["line"]), match["kind"] or match["what"],
                   match["name"]))
    return sorted(found)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("build", type=pathlib.Path, help="a configured build directory")
    parser.add_argument(
        "--output", type=pathlib.Path, default=ROOT / "build" / "site-api",
        help="the directory of the generated pages (default: build/site-api)")
    parser.add_argument(
        "--undocumented", action="store_true",
        help="list the public declarations without a comment instead")
    parser.add_argument(
        "--only", default="",
        help="with --undocumented, the headers under this path of include/easylocal")
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

    command = [
        mrdocs,
        f"--config={ROOT / 'docs' / 'mrdocs.yml'}",
        f"--compilation-database={work / 'compile_commands.json'}",
    ]
    if args.undocumented:
        listing = work / "undocumented"
        shutil.rmtree(listing, ignore_errors=True)
        log = subprocess.run(
            [*command, f"--output={listing}", "--warn-if-undocumented=true",
             "--warn-if-undoc-enum-val=true", "--warn-as-error=false"],
            capture_output=True, text=True)
        found = undocumented(log.stdout + log.stderr, args.only)
        for path, line, kind, name in found:
            print(f"{path}:{line}\t{kind}\t{name}")
        print(f"{len(found)} undocumented declarations", file=sys.stderr)
        return log.returncode

    output = args.output.resolve()
    shutil.rmtree(output, ignore_errors=True)
    return subprocess.call([*command, f"--output={output}"])


if __name__ == "__main__":
    sys.exit(main())
