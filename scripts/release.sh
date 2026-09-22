#!/usr/bin/env bash
#
# Prepare and publish an EasyLocal release.
#
# VERSION is the single source of truth. The release tag is always vMAJOR.MINOR.PATCH.
# The local release build is a pre-flight check; the full compiler/platform matrix
# runs remotely when the tag is pushed.
#
# Usage:
#   ./scripts/release.sh patch
#   ./scripts/release.sh minor
#   ./scripts/release.sh major
#   ./scripts/release.sh X.Y.Z
#   ./scripts/release.sh vX.Y.Z
#
# Options:
#   --yes, -y   do not ask for final confirmation

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
  --yes, -y   do not ask for final confirmation
USAGE
    exit 1
}

BUMP=""
ASSUME_YES=0
for arg in "$@"; do
    case "$arg" in
        --yes|-y) ASSUME_YES=1 ;;
        -h|--help) usage ;;
        *)
            [[ -z "$BUMP" ]] || usage
            BUMP="$arg"
            ;;
    esac
done
[[ -n "$BUMP" ]] || usage

cd "$(dirname "$0")/.."
[[ -d .git ]] || die "run this script from a Git checkout"
[[ -f VERSION ]] || die "VERSION not found"
[[ -f CMakeLists.txt ]] || die "CMakeLists.txt not found"
[[ -f CMakePresets.json ]] || die "CMakePresets.json not found"

command -v git >/dev/null 2>&1 || die "git not found"
command -v cmake >/dev/null 2>&1 || die "cmake not found"
command -v ctest >/dev/null 2>&1 || die "ctest not found"

OLD_VERSION="$(tr -d '[:space:]' < VERSION)"
[[ "$OLD_VERSION" =~ ^([0-9]+)\.([0-9]+)\.([0-9]+)$ ]] \
    || die "invalid VERSION: '$OLD_VERSION'"

MAJOR="${BASH_REMATCH[1]}"
MINOR="${BASH_REMATCH[2]}"
PATCH="${BASH_REMATCH[3]}"

case "$BUMP" in
    patch) NEW_VERSION="${MAJOR}.${MINOR}.$((PATCH + 1))" ;;
    minor) NEW_VERSION="${MAJOR}.$((MINOR + 1)).0" ;;
    major) NEW_VERSION="$((MAJOR + 1)).0.0" ;;
    *)
        NEW_VERSION="${BUMP#v}"
        [[ "$NEW_VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || usage
        ;;
esac

NEW_TAG="v${NEW_VERSION}"

echo
echo "EasyLocal release"
echo "-----------------"
echo "Current version : $OLD_VERSION"
echo "New version     : $NEW_VERSION"
echo "Tag             : $NEW_TAG"
echo

if [[ -n "$(git status --porcelain)" ]]; then
    git status --short
    die "working tree is not clean"
fi

CURRENT_BRANCH="$(git branch --show-current)"
[[ -n "$CURRENT_BRANCH" ]] || die "cannot determine current branch"

# Update remote tag knowledge before checking uniqueness.
git fetch --tags --prune origin

if git rev-parse -q --verify "refs/tags/$NEW_TAG" >/dev/null; then
    die "local tag '$NEW_TAG' already exists"
fi
if git ls-remote --exit-code --tags origin "refs/tags/$NEW_TAG" >/dev/null 2>&1; then
    die "remote tag '$NEW_TAG' already exists"
fi

printf '%s\n' "$NEW_VERSION" > VERSION

# Local release pre-flight: native macOS/AppleClang preset.
cmake --preset release
cmake --build --preset release --parallel 2
ctest --preset release

echo
echo "Changes:"
git status --short
git --no-pager diff -- VERSION CMakeLists.txt || true

if [[ "$ASSUME_YES" -eq 0 ]]; then
    echo
    read -r -p "Create and publish release $NEW_TAG? [y/N] " answer
    case "$answer" in
        y|Y|yes|YES|s|S|si|SI|sì) ;;
        *)
            echo "Release cancelled. VERSION remains changed in the working tree."
            exit 0
            ;;
    esac
fi

git add VERSION
if git diff --cached --quiet; then
    echo "VERSION is unchanged; tagging current commit."
else
    git commit -m "Release $NEW_TAG"
fi

git tag -a "$NEW_TAG" -m "EasyLocal $NEW_VERSION"
git push origin "$CURRENT_BRANCH"
git push origin "$NEW_TAG"

echo
echo "Published $NEW_TAG. GitHub Actions will run the full CI matrix for the tag."
