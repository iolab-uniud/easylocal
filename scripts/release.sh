#!/usr/bin/env bash
# Releases the version that VERSION holds: dates its CHANGELOG.md section,
# runs the Release build and its tests, then commits, tags, pushes and creates
# the GitHub release page.

set -euo pipefail

die() {
    echo "ERROR: $*" >&2
    exit 1
}

usage() {
    cat <<'USAGE'
Usage:
  ./scripts/release.sh [--yes] [--no-edit]
  ./scripts/release.sh --page-only

Releases the version in VERSION, MAJOR.MINOR.PATCH or
MAJOR.MINOR.PATCH-PRERELEASE (such as 4.0.0-alpha.1). CHANGELOG.md must have
its section, "## [VERSION] — not yet released", which becomes
"## [VERSION] — YYYY-MM-DD"; the links at the end of the file then compare
[Unreleased] with the new tag; CITATION.cff gets the version and the date.
It runs on main, up to date with origin/main. The Release build is
configured, built and tested, then the change is committed, tagged vVERSION
and pushed, and a GitHub release is created from the CHANGELOG.md section (a
pre-release for a PRERELEASE version). A failure or a cancel before the commit
restores CHANGELOG.md and CITATION.cff.

Afterwards, set VERSION to the next version in preparation and add its
section to CHANGELOG.md.

Options:
  --yes       do not ask for confirmation and do not open the editor
  --no-edit   do not open the editor to review CHANGELOG.md
  --page-only only create the GitHub release of the existing tag vVERSION

Environment:
  EDITOR / VISUAL / GIT_EDITOR
      editor used to review CHANGELOG.md (default: vi)
USAGE
    exit 1
}

ASSUME_YES=0
EDIT_CHANGELOG=1
PAGE_ONLY=0

for arg in "$@"; do
    case "$arg" in
        --yes|-y)
            ASSUME_YES=1
            ;;
        --no-edit)
            EDIT_CHANGELOG=0
            ;;
        --page-only)
            PAGE_ONLY=1
            ;;
        *)
            usage
            ;;
    esac
done

cd "$(dirname "$0")/.."

command -v git >/dev/null 2>&1 || die "'git' not found in PATH"
# A worktree has a .git file, not a directory: ask Git.
[[ "$(git rev-parse --show-toplevel 2>/dev/null)" == "$(pwd -P)" ]] \
    || die "this script must be run inside the Git repository"
[[ -f VERSION ]] || die "VERSION not found"
[[ -f CHANGELOG.md ]] || die "CHANGELOG.md not found"
[[ -f CMakePresets.json ]] || die "CMakePresets.json not found"

VERSION="$(tr -d '[:space:]' < VERSION)"
[[ "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+(-[0-9A-Za-z.-]+)?$ ]] \
    || die "VERSION must be MAJOR.MINOR.PATCH[-PRERELEASE], got '$VERSION'"
TAG="v${VERSION}"
HEADING="## [${VERSION}]"
REPOSITORY="https://github.com/iolab-uniud/easylocal"

# The CHANGELOG.md section of VERSION, without its heading, with the relative
# links made absolute at the tag so that they work on the release page.
release_notes() {
    awk -v heading="$HEADING" '
        index($0, heading " — ") == 1 { inside = 1; next }
        inside && (/^## \[/ || /^\[[^]]+\]: /) { exit }
        inside { print }
    ' CHANGELOG.md \
        | perl -pe "s{\]\((?!https?://|#)([^)]+)\)}{](${REPOSITORY}/blob/${TAG}/\$1)}g"
}

# The GitHub release of the tag, from the CHANGELOG.md section.
create_release_page() {
    local prerelease=()
    [[ "$VERSION" == *-* ]] && prerelease=(--prerelease)
    release_notes | gh release create "$TAG" --verify-tag \
        --title "EasyLocal $VERSION" ${prerelease[@]+"${prerelease[@]}"} \
        --notes-file -
}

echo
echo "EasyLocal release"
echo "-----------------"
echo "Version : $VERSION"
echo "Tag     : $TAG"
echo

command -v gh >/dev/null 2>&1 || die "'gh' not found in PATH"
command -v perl >/dev/null 2>&1 || die "'perl' not found in PATH"
gh auth status >/dev/null 2>&1 || die "'gh' is not logged in: run 'gh auth login'"

if [[ "$PAGE_ONLY" -eq 1 ]]; then
    git ls-remote --exit-code --tags origin "refs/tags/$TAG" >/dev/null 2>&1 \
        || die "remote tag '$TAG' does not exist"
    create_release_page
    echo "Created the GitHub release of $TAG."
    exit 0
fi

command -v cmake >/dev/null 2>&1 || die "'cmake' not found in PATH"
command -v ctest >/dev/null 2>&1 || die "'ctest' not found in PATH"

if [[ -n "$(git status --porcelain)" ]]; then
    git status --short
    die "working tree is not clean"
fi

# The release is made from main, up to date with origin/main: checked before
# anything is changed.
CURRENT_BRANCH="$(git branch --show-current)"
[[ "$CURRENT_BRANCH" == main ]] \
    || die "the release is made from main, not '${CURRENT_BRANCH:-a detached HEAD}'"

git fetch --tags --prune origin

[[ "$(git rev-parse HEAD)" == "$(git rev-parse origin/main)" ]] \
    || die "main is not origin/main: pull or push first"

if git rev-parse -q --verify "refs/tags/$TAG" >/dev/null; then
    die "local tag '$TAG' already exists: set VERSION to the next version"
fi
if git ls-remote --exit-code --tags origin "refs/tags/$TAG" >/dev/null 2>&1; then
    die "remote tag '$TAG' already exists: set VERSION to the next version"
fi

SECTION="$(grep -F -x -m 1 "$HEADING — not yet released" CHANGELOG.md || true)"
[[ -n "$SECTION" ]] \
    || die "CHANGELOG.md has no '$HEADING — not yet released' section"

TODAY="$(date +%Y-%m-%d)"

# Until the release commit, an exit (an error, a failed test, a cancel)
# restores CHANGELOG.md and CITATION.cff; the CHANGELOG.md reviewed in the
# editor is kept in a temporary file.
RESTORE=0
TMP_CHANGELOG="$(mktemp)"
TMP_CITATION="$(mktemp)"
restore() {
    rm -f "$TMP_CHANGELOG" "$TMP_CITATION"
    [[ "$RESTORE" -eq 1 ]] || return 0
    local kept
    kept="$(mktemp "${TMPDIR:-/tmp}/CHANGELOG.XXXXXX")"
    cp CHANGELOG.md "$kept"
    git checkout -- CHANGELOG.md
    [[ -f CITATION.cff ]] && git checkout -- CITATION.cff
    echo "CHANGELOG.md and CITATION.cff are restored; the reviewed CHANGELOG.md is $kept." >&2
}
trap restore EXIT

# The section is dated; [Unreleased] compares with the new tag, and the
# version gets its link when it has none.
RESTORE=1
awk -v heading="$HEADING" -v today="$TODAY" -v tag="$TAG" \
    -v version="$VERSION" -v repository="$REPOSITORY" '
    $0 == heading " — not yet released" { print heading " — " today; next }
    /^\[Unreleased\]: / {
        print "[Unreleased]: " repository "/compare/" tag "...HEAD"
        unreleased = 1
        next
    }
    index($0, "[" version "]: ") == 1 { linked = 1 }
    { print }
    END {
        if (!unreleased)
            print "[Unreleased]: " repository "/compare/" tag "...HEAD"
        if (!linked)
            print "[" version "]: " repository "/releases/tag/" tag
    }
' CHANGELOG.md > "$TMP_CHANGELOG"
mv "$TMP_CHANGELOG" CHANGELOG.md

# CITATION.cff cites the released version.
if [[ -f CITATION.cff ]]; then
    awk -v version="$VERSION" -v today="$TODAY" '
        /^date-released:/ { next }
        /^version:/ { print "version: " version; print "date-released: " today; next }
        { print }
    ' CITATION.cff > "$TMP_CITATION"
    mv "$TMP_CITATION" CITATION.cff
fi

if [[ "$ASSUME_YES" -eq 0 && "$EDIT_CHANGELOG" -eq 1 && -t 1 ]]; then
    EDITOR_CMD="${GIT_EDITOR:-${VISUAL:-${EDITOR:-vi}}}"
    $EDITOR_CMD CHANGELOG.md </dev/tty >/dev/tty 2>&1 \
        || die "editor exited with an error"
fi

echo
echo "Running local release verification..."

cmake --preset release
cmake --build --preset release --parallel
ctest --preset release --output-on-failure

echo
git --no-pager diff -- CHANGELOG.md CITATION.cff

if [[ "$ASSUME_YES" -eq 0 ]]; then
    echo
    read -r -p "Create and publish release $TAG? [y/N] " answer

    case "$answer" in
        y|Y|yes|YES)
            ;;
        *)
            echo "Release cancelled."
            exit 0
            ;;
    esac
fi

git add CHANGELOG.md CITATION.cff
git commit -m "chore(release): $TAG"
RESTORE=0
git tag -a "$TAG" -m "EasyLocal $VERSION"

git push origin main
git push origin "$TAG"
create_release_page

echo
echo "Released $TAG. Next: set VERSION to the next version in preparation and"
echo "add its '## [NEXT] — not yet released' section to CHANGELOG.md."
