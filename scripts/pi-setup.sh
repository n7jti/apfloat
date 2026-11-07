#!/usr/bin/env bash
set -euo pipefail
umask 077

fail() { printf 'Error: %s\n' "$*" >&2; exit 1; }

[[ -t 0 && -t 1 ]] || fail "Run setup in an interactive terminal; do not record its input."
[[ $(id -u) != 0 ]] || fail "Run as a normal user, not root."
[[ $(uname -s) == Linux && $(uname -m) == aarch64 ]] || fail "64-bit ARM Linux is required."
[[ -f /etc/debian_version ]] && command -v apt-get >/dev/null || fail "A Debian-based OS with apt-get is required."
[[ -n ${HOME:-} && -d $HOME ]] || fail "A valid home directory is required."

runner_dir="$HOME/.local/share/apfloat/actions-runner"
if [[ -e $runner_dir || -L $runner_dir ]]; then
    [[ -d $runner_dir && ! -L $runner_dir ]] || fail "Runner path is not a private directory."
    [[ -O $runner_dir ]] || fail "Runner directory must be owned by the current user."
    existing=$(cd "$runner_dir" && pwd -P)
    project=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)
    [[ $existing != "$project" && $existing != "$project/"* ]] || fail "Runner files must be outside the project checkout."
    if [[ -f $runner_dir/.runner && -f $runner_dir/.credentials && -x $runner_dir/run.sh ]]; then
        [[ ! -L $runner_dir/.credentials && -O $runner_dir/.credentials ]] || fail "Runner credentials must be owned by the current user and not be a symlink."
        mode=$(stat -c '%a' "$runner_dir/.credentials")
        (( (8#$mode & 077) == 0 )) || fail "Runner credentials are accessible to other users; secure them before continuing."
        printf '%s\n' "Existing runner found; preserving it. Verify its repository and raspberry-pi label in GitHub Settings."
        exit 0
    fi
    fail "Runner directory already exists but is incomplete. Inspect it manually; nothing was replaced."
fi

command -v dpkg-query >/dev/null || fail "dpkg-query is required."
script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)
source "$script_dir/pi-packages.sh"
pi_missing_packages
((${#missing[@]} == 0)) || fail "Missing packages: ${missing[*]}. Ask an administrator to run scripts/pi-create-user.sh or install them."
for command in cmake c++ git curl jq tar sha256sum df; do
    command -v "$command" >/dev/null || fail "Required command is unavailable: $command"
done
version=$(cmake --version | head -n 1)
[[ $version =~ ([0-9]+)\.([0-9]+)\.([0-9]+) ]] || fail "Cannot determine CMake version."
major=${BASH_REMATCH[1]}
minor=${BASH_REMATCH[2]}
((10#$major > 3 || (10#$major == 3 && 10#$minor >= 20))) || fail "CMake 3.20 or newer is required."
available=$(df -Pk "$HOME" | awk 'NR==2 {print $4}')
[[ $available =~ ^[0-9]+$ ]] && ((available >= 1048576)) || fail "At least 1 GiB of free space is required in the home filesystem."

base="$HOME/.local/share/apfloat"
[[ ! -L $base ]] || fail "The runner parent directory must not be a symlink."
mkdir -p "$base"
[[ -O $base ]] || fail "The runner parent directory must be owned by the current user."
base=$(cd "$base" && pwd -P)
project=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)
[[ $base != "$project" && $base != "$project/"* ]] || fail "Runner files must be outside the project checkout."
chmod 700 "$base"
tmp=$(mktemp -d "$base/.runner-download.XXXXXXXX")
trap 'rm -rf -- "$tmp"' EXIT

printf '%s\n' 'int main() { return 0; }' > "$tmp/compiler.cpp"
c++ -std=c++17 "$tmp/compiler.cpp" -o "$tmp/compiler-check" || fail "The C++ compiler cannot compile a C++17 program."

curl --fail --silent --show-error --location --proto '=https' --proto-redir '=https' \
    https://api.github.com/repos/actions/runner/releases/latest -o "$tmp/release.json"
tag=$(jq -er '.tag_name | select(type == "string" and test("^v[0-9]+\\.[0-9]+\\.[0-9]+$"))' "$tmp/release.json") || fail "Invalid official release version."
version=${tag#v}
asset="actions-runner-linux-arm64-${version}.tar.gz"
digest=$(jq -er --arg name "$asset" '[.assets[] | select(.name == $name) | .digest] | select(length == 1) | .[0] | select(type == "string" and test("^sha256:[a-f0-9]{64}$"))' "$tmp/release.json") || fail "Official SHA-256 digest is missing or invalid."
url="https://github.com/actions/runner/releases/download/$tag/$asset"
curl --fail --silent --show-error --location --proto '=https' --proto-redir '=https' "$url" -o "$tmp/$asset"
printf '%s  %s\n' "${digest#sha256:}" "$tmp/$asset" | sha256sum --check --status || fail "Runner archive checksum mismatch."

mkdir "$runner_dir"
tar -xzf "$tmp/$asset" -C "$runner_dir"
[[ -x $runner_dir/config.sh && -x $runner_dir/run.sh ]] || fail "Runner archive is incomplete; inspect the installation directory."
printf '%s\n' \
    "Enter the repository URL and short-lived registration token at the runner prompts." \
    "Get the token from repository Settings > Actions > Runners > New self-hosted runner." \
    "Do not enter the token in a shell command or record this terminal session."
cd "$runner_dir"
./config.sh --labels raspberry-pi
[[ -f .runner && -f .credentials ]] || fail "Registration did not produce the expected runner files."
[[ ! -L .credentials && -O .credentials ]] || fail "Runner credentials must be owned by the current user and not be a symlink."
mode=$(stat -c '%a' .credentials)
(( (8#$mode & 077) == 0 )) || fail "Runner credentials are accessible to other users; secure them before continuing."
printf '%s\n' "Runner registered. Start it manually with scripts/pi-session.sh."
