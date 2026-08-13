#!/bin/sh
#
# Show or bump the SMACK! fork version.
#
# The version lives in exactly one place, version.c:
#
#     const char smack_version[] = "X.Y.Z";
#
#   tools/bump-version.sh show      print the current version
#   tools/bump-version.sh patch     X.Y.Z -> X.Y.(Z+1)      small change
#   tools/bump-version.sh minor     X.Y.Z -> X.(Y+1).0      larger feature
#   tools/bump-version.sh major     X.Y.Z -> (X+1).0.0
#   tools/bump-version.sh set 1.2.3
#
# Normally you do not run this by hand -- tools/hooks/pre-commit calls it on
# every commit. See tools/install-hooks.sh.
#
# Portability: POSIX sh only, no bashisms, and no `sed -i` (GNU and BSD sed
# disagree about its argument), so the rewrite goes via a temp file. That makes
# it behave identically on Linux, macOS and Windows under Git's bundled sh.

set -e

# Locate version.c relative to the repository, not the caller's directory.
if repo=$(git rev-parse --show-toplevel 2>/dev/null); then
    :
else
    repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
fi
file="$repo/version.c"

[ -f "$file" ] || { echo "bump-version: no such file: $file" >&2; exit 1; }

pattern='^const char smack_version\[\] *= *"\([0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\)".*'
cur=$(sed -n "s/$pattern/\\1/p" "$file" | head -n 1)

if [ -z "$cur" ]; then
    echo "bump-version: could not find smack_version[] in $file" >&2
    echo "bump-version: expected  const char smack_version[] = \"X.Y.Z\";" >&2
    exit 1
fi

action=${1:-show}

if [ "$action" = show ]; then
    echo "$cur"
    exit 0
fi

# Split X.Y.Z without arrays or `cut` subprocesses.
major=${cur%%.*}
rest=${cur#*.}
minor=${rest%%.*}
patch=${rest##*.}

case "$action" in
    patch) patch=$((patch + 1)) ;;
    minor) minor=$((minor + 1)); patch=0 ;;
    major) major=$((major + 1)); minor=0; patch=0 ;;
    set)
        new=$2
        case "$new" in
            [0-9]*.[0-9]*.[0-9]*) ;;
            *) echo "bump-version: 'set' needs a version like 1.2.3" >&2; exit 1 ;;
        esac
        major=${new%%.*}; rest=${new#*.}; minor=${rest%%.*}; patch=${rest##*.}
        ;;
    *)
        echo "usage: $0 show|patch|minor|major|set X.Y.Z" >&2
        exit 1
        ;;
esac

new="$major.$minor.$patch"

tmp="$file.bump.$$"
sed "s/\\(^const char smack_version\\[\\] *= *\"\\)[0-9.]*\\(\".*\\)$/\\1$new\\2/" \
    "$file" > "$tmp"

# Refuse to write a file the substitution failed on, rather than truncating it.
if ! grep -q "smack_version\[\] *= *\"$new\"" "$tmp"; then
    rm -f "$tmp"
    echo "bump-version: rewrite failed, $file left untouched" >&2
    exit 1
fi

mv "$tmp" "$file"
echo "$cur -> $new"
