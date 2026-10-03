"""MkDocs hook: publish example sources as downloadable files.

Each entry of DOWNLOADS is a folder of the site, under downloads/, with the
files it serves and a zip of them all, so that a page can link the program it
shows, e.g. [main.cpp](downloads/quickstart/main.cpp){: download="main.cpp" }.
The files are read from examples/ at build time, so the downloads are the code
the test suite runs; the "// [section]" marker lines of the documentation
snippets are dropped, as on the pages.
"""

import io
import pathlib
import re
import zipfile

from mkdocs.structure.files import File

ROOT = pathlib.Path(__file__).resolve().parent.parent
EXAMPLES = ROOT / "examples"
MARKER = re.compile(r"\s*//\s*\[[\w-]+\]")  # as in sync-doc-snippets.py

# site folder -> {published name: source under examples/}
DOWNLOADS = {
    "quickstart": {
        "main.cpp": "quickstart/main.cpp",
        "CMakeLists.txt": "quickstart/standalone/CMakeLists.txt",
    },
}


def on_files(files, config):
    for folder, sources in DOWNLOADS.items():
        archive = io.BytesIO()
        with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as zipped:
            for name, source in sources.items():
                lines = (EXAMPLES / source).read_text(encoding="utf-8").splitlines(True)
                content = "".join(l for l in lines if not MARKER.match(l)).encode()
                files.append(
                    File.generated(config, f"downloads/{folder}/{name}", content=content))
                zipped.writestr(f"{folder}/{name}", content)
        files.append(
            File.generated(
                config, f"downloads/{folder}/{folder}.zip", content=archive.getvalue()))
    return files
