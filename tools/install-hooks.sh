#!/bin/sh
#
# Point this clone's git hooks at tools/hooks, which is version-controlled --
# unlike .git/hooks, which is not and so cannot be shared.
#
#   sh tools/install-hooks.sh
#
# One command, same on Linux, macOS and Windows (Git for Windows understands
# core.hooksPath and runs the hooks with its bundled sh).
#
# To undo:      git config --unset core.hooksPath
# To skip once: SMACK_NO_BUMP=1 git commit ...

set -e

repo=$(git rev-parse --show-toplevel)
cd "$repo"

git config core.hooksPath tools/hooks

# Make sure the exec bit is set both on disk and in the index. Windows
# checkouts often have core.filemode=false, in which case git ignores the
# on-disk bit and the index copy is what matters.
chmod +x tools/hooks/* tools/*.sh 2>/dev/null || true
git update-index --chmod=+x tools/hooks/pre-commit 2>/dev/null || true
git update-index --chmod=+x tools/bump-version.sh 2>/dev/null || true
git update-index --chmod=+x tools/install-hooks.sh 2>/dev/null || true

echo "hooks installed: core.hooksPath = $(git config core.hooksPath)"
echo "current version: $(sh tools/bump-version.sh show)"
