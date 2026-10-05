#!/usr/bin/env python3
"""Generate the API reference with MrDocs, into build/site-api.

MrDocs reads the headers through a compilation database. The script writes one
with a single translation unit that includes every public header, and the
adapters (TOML, TUI, REST) enabled in the build's CMakeCache.txt. The flags
are the include paths and definitions of the compilation database of that
build, so that the adapters' dependencies are found; configure it with every
optional component for a complete reference:

    cmake --preset dev -B build/api -DEASYLOCAL_ENABLE_CONFIG_TOML=ON \\
        -DEASYLOCAL_ENABLE_TUI=ON -DEASYLOCAL_ENABLE_REST=ON \\
        -DEASYLOCAL_FETCH_DEPENDENCIES=ON
    uv run scripts/api-docs.py build/api

The build fails on a MrDocs warning (a malformed comment), on a public
declaration without a comment (read from the XML that MrDocs writes with the
pages, which also covers the members of class template specializations) and
on a link to a page that was not generated.

With --undocumented it writes no pages but lists the public declarations
without a comment, as file:line, kind and name, optionally only those of the
headers under --only (a path relative to include/easylocal, such as trace/):

    uv run scripts/api-docs.py build/api --undocumented --only trace/

The pages have the look of the documentation site (docs/mrdocs/: the page
layout and the stylesheet) and link to it as their parent directory, as the
site serves them under api/.

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
import xml.etree.ElementTree as ET

ROOT = pathlib.Path(__file__).resolve().parent.parent
INCLUDE = ROOT / "include"
# adapter header -> the CMake option that enables it
ADAPTERS = {
    "easylocal/adapters/toml.hpp": "EASYLOCAL_ENABLE_CONFIG_TOML",
    "easylocal/adapters/tui.hpp": "EASYLOCAL_ENABLE_TUI",
    "easylocal/adapters/rest.hpp": "EASYLOCAL_ENABLE_REST",
}


def public_headers():
    """The headers of include/easylocal, outside detail/ and the adapters."""
    for path in sorted((INCLUDE / "easylocal").rglob("*.hpp")):
        relative = path.relative_to(INCLUDE).as_posix()
        if "/detail/" in relative or relative.startswith("easylocal/adapters"):
            continue
        yield relative


def enabled_adapters(build):
    """The adapter headers whose option is ON in the build's CMakeCache.txt."""
    cache = build / "CMakeCache.txt"
    options = {}
    if cache.is_file():
        for line in cache.read_text().splitlines():
            match = re.match(r"(\w+):BOOL=(.*)$", line)
            if match:
                options[match[1]] = match[2].upper() in ("ON", "TRUE", "1", "YES")
    return [adapter for adapter, option in ADAPTERS.items() if options.get(option)]


def translation_unit(adapters):
    return "".join(f"#include <{header}>\n" for header in [*public_headers(), *adapters])


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


def location(element):
    """The header (relative to include/easylocal) and line of a symbol."""
    for loc in element.iterfind("loc//sourcePath/.."):
        path = loc.findtext("sourcePath")
        if path.startswith("easylocal/"):
            return path[len("easylocal/"):], int(loc.findtext("lineNumber") or 0)
    return "", 0


def undocumented(reference, only):
    """The public symbols of the XML reference without a comment.

    Namespaces are left out: they have no page of their own to describe."""
    symbols = {element.findtext("id"): element for element in reference.getroot()}

    def qualified(element):
        names = []
        while element is not None and element.findtext("name"):
            names.append(element.findtext("name"))
            element = symbols.get(element.findtext("parent"))
        return "::".join(reversed(names))

    found = set()
    for element in symbols.values():
        if (element.tag == "namespace" or element.find("doc") is not None
                or element.findtext("extraction") != "regular"
                or element.findtext("access") in ("private", "protected")):
            continue
        path, line = location(element)
        if not path or "/detail/" in path or not path.startswith(only):
            continue
        kind = element.findtext("funcClass") or ""
        kind = element.tag if kind in ("", "normal") else kind
        found.add((path, line, kind, qualified(element)))
    return sorted(found)


def broken_links(pages):
    """The links of the pages to a page under pages that does not exist."""
    found = set()
    for page in sorted(pages.rglob("*.html")):
        for href in re.findall(r'href="([^"#?]*)', page.read_text()):
            if not href or re.match(r"[a-z][a-z0-9+.-]*:", href):
                continue
            target = (page.parent / href).resolve()
            # links out of the pages go to the documentation site
            if target.is_relative_to(pages) and not target.exists():
                found.add((page.relative_to(pages).as_posix(),
                           target.relative_to(pages).as_posix()))
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

    build = args.build.resolve()
    database = build / "compile_commands.json"
    if not database.is_file():
        sys.exit(f"{database} not found: configure the build first")
    mrdocs = os.environ.get("MRDOCS") or shutil.which("mrdocs")
    if not mrdocs:
        sys.exit("mrdocs not found: set MRDOCS or put it on the PATH")

    adapters = enabled_adapters(build)
    for adapter, option in ADAPTERS.items():
        if adapter not in adapters:
            print(f"note: {option} is not ON in {build}: no {adapter} in the reference",
                  file=sys.stderr)

    work = build / "api-docs"
    work.mkdir(parents=True, exist_ok=True)
    source = work / "easylocal_api.cpp"
    source.write_text(translation_unit(adapters))
    arguments = ["clang++", "-std=c++23", *build_flags(database), "-c", str(source)]
    (work / "compile_commands.json").write_text(json.dumps(
        [{"directory": str(work), "file": str(source), "arguments": arguments}], indent=1))

    generated = work / "generated"
    shutil.rmtree(generated, ignore_errors=True)
    command = [
        mrdocs,
        f"--config={ROOT / 'docs' / 'mrdocs.yml'}",
        f"--compilation-database={work / 'compile_commands.json'}",
        f"--output={generated}",
        "--generator=html,xml",
    ]
    if args.undocumented:
        command.append("--warn-as-error=false")
    log = subprocess.run(command, capture_output=True, text=True)
    if log.returncode != 0:
        sys.stdout.write(log.stdout)
        sys.stderr.write(log.stderr)
        print(f"MrDocs failed with exit status {log.returncode}", file=sys.stderr)
        return log.returncode

    found = undocumented(ET.parse(generated / "xml" / "reference.xml"), args.only)
    for path, line, kind, name in found:
        print(f"{path}:{line}\t{kind}\t{name}")
    if args.undocumented:
        print(f"{len(found)} undocumented declarations", file=sys.stderr)
        return 0
    if found:
        print(f"{len(found)} public declarations without a comment", file=sys.stderr)
        return 1

    pages = generated / "html"
    links = broken_links(pages)
    for page, target in links:
        print(f"{page}: link to the missing page {target}", file=sys.stderr)
    if links:
        print(f"{len(links)} links to missing pages", file=sys.stderr)
        return 1

    output = args.output.resolve()
    shutil.rmtree(output, ignore_errors=True)
    shutil.move(pages, output)
    count = sum(1 for _ in output.rglob("*.html"))
    print(f"{count} pages in {output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
