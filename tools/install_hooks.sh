#!/bin/sh
# Installs tools/hooks/pre-commit and commit-msg into this clone (git config core.hooksPath).
root=$(cd "$(dirname "$0")/.." && pwd)
chmod +x "$root/tools/hooks/pre-commit" "$root/tools/hooks/commit-msg"
git -C "$root" config core.hooksPath tools/hooks && echo "hooks installed: core.hooksPath = tools/hooks"
