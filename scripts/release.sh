#!/usr/bin/env bash

set -euo pipefail

die() {
    echo "ERROR: $*" >&2
    exit 1
}

usage() {
    cat <<'USAGE'
Usage:
  ./scripts/release.sh patch
  ./scripts/release.sh minor
  ./scripts/release.sh major
  ./scripts/release.sh X.Y.Z
  ./scripts/release.sh vX.Y.Z

Options:
  --yes       do not ask for confirmation and do not open the editor
  --no-llm    do not ask an LLM to draft the changelog entry
  --no-edit   do not open the editor for changelog review

Environment:
  RELEASE_CHANGELOG_CMD
      command used to draft the changelog entry from stdin to stdout.
      If unset, "claude -p" is used when available.

  EDITOR / VISUAL / GIT_EDITOR
      editor used to review the changelog entry (default: vi)
USAGE
    exit 1
}

BUMP=""
ASSUME_YES=0
USE_LLM=1
EDIT_CHANGELOG=1

for arg in "$@"; do
    case "$arg" in
        --yes|-y)
            ASSUME_YES=1
            ;;
        --no-llm)
            USE_LLM=0
            ;;
        --no-edit)
            EDIT_CHANGELOG=0
            ;;
        -h|--help)
            usage
            ;;
        *)
            [[ -z "$BUMP" ]] || usage
            BUMP="$arg"
            ;;
    esac
done

[[ -n "$BUMP" ]] || usage

cd "$(dirname "$0")/.."

[[ -d .git ]] || die "this script must be run inside the Git repository"
[[ -f VERSION ]] || die "VERSION not found"
[[ -f CHANGELOG.md ]] || die "CHANGELOG.md not found"
[[ -f CMakePresets.json ]] || die "CMakePresets.json not found"

OLD_VERSION="$(tr -d '[:space:]' < VERSION)"
[[ "$OLD_VERSION" =~ ^([0-9]+)\.([0-9]+)\.([0-9]+)$ ]] \
    || die "invalid version in VERSION: '$OLD_VERSION'"

MAJOR="${BASH_REMATCH[1]}"
MINOR="${BASH_REMATCH[2]}"
PATCH="${BASH_REMATCH[3]}"

case "$BUMP" in
    patch)
        NEW_VERSION="${MAJOR}.${MINOR}.$((PATCH + 1))"
        ;;
    minor)
        NEW_VERSION="${MAJOR}.$((MINOR + 1)).0"
        ;;
    major)
        NEW_VERSION="$((MAJOR + 1)).0.0"
        ;;
    *)
        NEW_VERSION="${BUMP#v}"
        [[ "$NEW_VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || usage
        ;;
esac

NEW_TAG="v${NEW_VERSION}"

echo
echo "EasyLocal++ release"
echo "-------------------"
echo "Current version : $OLD_VERSION"
echo "New version     : $NEW_VERSION"
echo "Tag             : $NEW_TAG"
echo

command -v git >/dev/null 2>&1 || die "'git' not found in PATH"
command -v cmake >/dev/null 2>&1 || die "'cmake' not found in PATH"
command -v ctest >/dev/null 2>&1 || die "'ctest' not found in PATH"

if [[ -n "$(git status --porcelain)" ]]; then
    git status --short
    die "working tree is not clean"
fi

git fetch --tags --prune origin

LAST_RELEASE_VERSION="$(
    git tag --list 'v[0-9]*.[0-9]*.[0-9]*' |
    sed 's/^v//' |
    sort -V |
    tail -1
)"

if [[ -n "$LAST_RELEASE_VERSION" ]]; then
    HIGHEST="$(
        printf '%s\n%s\n' "$LAST_RELEASE_VERSION" "$OLD_VERSION" |
        sort -V |
        tail -1
    )"

    if [[ "$HIGHEST" != "$OLD_VERSION" ]]; then
        die "VERSION declares $OLD_VERSION but latest release tag is v$LAST_RELEASE_VERSION"
    fi
fi

if git rev-parse -q --verify "refs/tags/$NEW_TAG" >/dev/null; then
    die "local tag '$NEW_TAG' already exists"
fi

if git ls-remote --exit-code --tags origin "refs/tags/$NEW_TAG" >/dev/null 2>&1; then
    die "remote tag '$NEW_TAG' already exists"
fi

printf '%s\n' "$NEW_VERSION" > VERSION

TODAY="$(date +%Y-%m-%d)"

LAST_TAG="$(git describe --tags --abbrev=0 --match 'v[0-9]*.[0-9]*.[0-9]*' 2>/dev/null || true)"

if [[ -n "$LAST_TAG" ]]; then
    RANGE="${LAST_TAG}..HEAD"
    echo "Preparing changelog from $LAST_TAG to HEAD."
else
    RANGE="HEAD"
    echo "No previous release tag: preparing changelog from repository history."
fi

COMMITS="$(git log --no-merges --pretty=format:'- %s' "$RANGE" || true)"
DIFFSTAT="$(git diff --stat "$RANGE" 2>/dev/null | tail -n 40 || true)"

ENTRY="$(mktemp)"
trap 'rm -f "$ENTRY"' EXIT

DRAFT=""

if [[ "$USE_LLM" -eq 1 && -n "$COMMITS" ]]; then
    LLM_CMD="${RELEASE_CHANGELOG_CMD:-}"

    if [[ -z "$LLM_CMD" ]] && command -v claude >/dev/null 2>&1; then
        LLM_CMD="claude -p"
    fi

    if [[ -n "$LLM_CMD" ]]; then
        echo "Drafting changelog entry with: $LLM_CMD"

        PROMPT="Write the CHANGELOG entry for EasyLocal++ version $NEW_VERSION.

EasyLocal++ is a modern C++23 header-only framework for local search and
metaheuristics.

Rules:
- reply ONLY with a Markdown bullet list;
- write in English;
- describe user-visible API, behavior, build, testing, packaging, portability,
  or migration changes;
- group commits that belong to the same logical change;
- omit internal refactoring and typo fixes unless they affect users;
- do not invent features that are not supported by the commit history;
- keep each bullet concise but specific;
- mention breaking changes explicitly when present.

Commits since the previous release:
$COMMITS

Files changed:
$DIFFSTAT"

        DRAFT="$(printf '%s' "$PROMPT" | $LLM_CMD 2>/dev/null || true)"

        if [[ -z "$DRAFT" ]]; then
            echo "  (LLM produced no draft; falling back to commit subjects)"
        fi
    fi
fi

if [[ -n "$DRAFT" ]]; then
    printf '%s\n' "$DRAFT" > "$ENTRY"
elif [[ -n "$COMMITS" ]]; then
    printf '%s\n' "$COMMITS" > "$ENTRY"
else
    printf -- '- Release %s.\n' "$NEW_VERSION" > "$ENTRY"
fi

if [[ "$ASSUME_YES" -eq 0 && "$EDIT_CHANGELOG" -eq 1 && -t 1 ]]; then
    {
        echo
        printf '# EasyLocal++ %s (%s)\n' "$NEW_VERSION" "$TODAY"
        printf '# Review the entry carefully before tagging the release.\n'
        printf '# Lines beginning with # are ignored.\n'
        printf '# Save and quit to continue; empty the entry to cancel.\n'
    } >> "$ENTRY"

    EDITOR_CMD="${GIT_EDITOR:-${VISUAL:-${EDITOR:-vi}}}"

    $EDITOR_CMD "$ENTRY" </dev/tty >/dev/tty 2>&1 \
        || die "editor exited with an error"

    CLEANED="$(
        grep -v '^#' "$ENTRY" |
        perl -0pe 's/\A\s*\n//; s/\s*\z/\n/' || true
    )"
    CLEANED="${CLEANED%$'\n'}"

    [[ -n "$CLEANED" ]] || die "empty changelog entry: release cancelled"

    printf '%s\n' "$CLEANED" > "$ENTRY"
fi

TMP_CHANGELOG="$(mktemp)"

{
    printf '# Changelog\n\n'
    printf 'All notable changes to EasyLocal++ will be documented in this file.\n\n'
    printf 'The project uses semantic versioning. Release entries are prepared from the\n'
    printf 'commits since the previous release and are reviewed manually before tagging.\n\n'
    printf '## %s - %s\n\n' "$NEW_VERSION" "$TODAY"
    cat "$ENTRY"
    printf '\n'

    awk '
        BEGIN { skipping_header = 1 }
        skipping_header && /^# Changelog$/ { next }
        skipping_header && /^All notable changes to EasyLocal\+\+/ { next }
        skipping_header && /^The project uses semantic versioning\./ { next }
        skipping_header && /^commits since the previous release/ { next }
        skipping_header && /^$/ { next }
        { skipping_header = 0; print }
    ' CHANGELOG.md
} > "$TMP_CHANGELOG"

mv "$TMP_CHANGELOG" CHANGELOG.md

echo
echo "Running local release verification..."

cmake --preset release
cmake --build --preset release --parallel
ctest --preset release --output-on-failure

echo
echo "Release changes:"
git status --short
echo
git --no-pager diff -- VERSION CHANGELOG.md

if [[ "$ASSUME_YES" -eq 0 ]]; then
    echo
    read -r -p "Create and publish release $NEW_TAG? [y/N] " answer

    case "$answer" in
        y|Y|yes|YES)
            ;;
        *)
            echo "Release cancelled. VERSION and CHANGELOG.md changes remain in the working tree."
            exit 0
            ;;
    esac
fi

git add VERSION CHANGELOG.md

git commit -m "Release $NEW_TAG"

git tag -a "$NEW_TAG" -m "EasyLocal++ $NEW_VERSION"

CURRENT_BRANCH="$(git branch --show-current)"
[[ -n "$CURRENT_BRANCH" ]] || die "cannot determine current branch"

git push origin "$CURRENT_BRANCH"
git push origin "$NEW_TAG"

echo
echo "Published $NEW_TAG."
echo "The GitHub Actions release-tag workflow will now run the full C++23 CI matrix."
