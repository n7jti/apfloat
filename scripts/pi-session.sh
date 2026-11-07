#!/usr/bin/env bash
set -euo pipefail
umask 077

fail() { printf 'Error: %s\n' "$*" >&2; exit 1; }
[[ -t 0 && -t 1 ]] || fail "Run the session in an interactive terminal."
[[ $(id -u) != 0 ]] || fail "Run the runner as a normal user, not root."
[[ $(uname -s) == Linux && $(uname -m) == aarch64 ]] || fail "64-bit ARM Linux is required."
[[ -n ${HOME:-} ]] || fail "A valid home directory is required."
runner_dir="$HOME/.local/share/apfloat/actions-runner"
[[ -d $runner_dir && ! -L $runner_dir && -f $runner_dir/.runner && -f $runner_dir/.credentials && -x $runner_dir/run.sh ]] ||
    fail "Runner is not registered at the expected location; run scripts/pi-setup.sh first."
[[ -O $runner_dir ]] || fail "Runner directory must be owned by the current user."
[[ ! -L $runner_dir/.credentials && -O $runner_dir/.credentials ]] || fail "Runner credentials must be owned by the current user and not be a symlink."
mode=$(stat -c '%a' "$runner_dir/.credentials")
(( (8#$mode & 077) == 0 )) || fail "Runner credentials are accessible to other users; secure them before continuing."
existing=$(cd "$runner_dir" && pwd -P)
project=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)
[[ $existing != "$project" && $existing != "$project/"* ]] || fail "Runner files must be outside the project checkout."
printf '%s\n' \
    "Runner starting in the foreground. Manually dispatch Raspberry Pi on the default branch in GitHub Actions." \
    "Keep this terminal open until the job finishes, then press Ctrl+C to stop the runner."
cd "$runner_dir"
exec ./run.sh
