#!/usr/bin/env bash
set -euo pipefail
umask 077

fail() { printf 'Error: %s\n' "$*" >&2; exit 1; }
[[ -t 0 && -t 1 ]] || fail "Run provisioning in an interactive terminal."
[[ $(uname -s) == Linux && $(uname -m) == aarch64 ]] || fail "64-bit ARM Linux is required."
[[ -f /etc/debian_version ]] && command -v apt-get >/dev/null || fail "A Debian-based OS with apt-get is required."
if [[ $(id -u) != 0 ]]; then
    command -v sudo >/dev/null || fail "sudo is required for administrator provisioning."
    exec sudo -- "$0"
fi

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)
source "$script_dir/pi-packages.sh"
command -v dpkg-query >/dev/null || fail "dpkg-query is required."

verify_account() {
local entry name uid gid home shell status sudo_output sudo_status
entry=$(getent passwd gitrunr) || fail "Account has no passwd entry."
IFS=: read -r name _ uid gid _ home shell <<< "$entry"
[[ $name == gitrunr && $uid =~ ^[0-9]+$ && $uid != 0 && $home == /home/gitrunr && $shell == /bin/bash ]] ||
    fail "Account properties are unexpected; inspect gitrunr manually."
[[ -d $home && ! -L $home && $(stat -c '%u:%a' "$home") == "$uid:700" ]] ||
    fail "Runner home ownership or permissions are unexpected; inspect it manually."
[[ ! -e $home/.ssh/authorized_keys && ! -L $home/.ssh/authorized_keys && ! -e $home/.ssh/authorized_keys2 && ! -L $home/.ssh/authorized_keys2 ]] ||
    fail "Runner home has SSH authorized keys; inspect access manually."
[[ $(id -G gitrunr) == "$gid" ]] || fail "Runner has unexpected supplementary groups; inspect them manually."
status=$(passwd -S gitrunr)
[[ $status == "gitrunr L "* ]] || fail "Runner password is not locked; inspect it manually."
set +e
sudo_output=$(LC_ALL=C sudo -n -l -U gitrunr 2>&1)
sudo_status=$?
set -e
[[ $sudo_status == 1 && $sudo_output == *"not allowed to run sudo"* ]] ||
    fail "Could not verify that gitrunr has no sudo access; inspect sudo policy manually."
}

exists=false
if getent passwd gitrunr >/dev/null; then
    verify_account
    exists=true
    printf '%s\n' "Existing gitrunr account verified and preserved."
elif getent group gitrunr >/dev/null || [[ -e /home/gitrunr || -L /home/gitrunr ]]; then
    fail "The gitrunr group or home already exists; inspect it manually. Nothing was replaced."
fi

pi_missing_packages
if ((${#missing[@]})); then
    printf 'Installing missing packages: %s\n' "${missing[*]}"
    apt-get update
    apt-get install -y "${missing[@]}"
    pi_missing_packages
    ((${#missing[@]} == 0)) || fail "Packages remain missing: ${missing[*]}"
fi

if [[ $exists == false ]]; then
    useradd -m -U -s /bin/bash -p '!' gitrunr
    chmod 700 /home/gitrunr
    verify_account
    printf '%s\n' "Created gitrunr with a locked password, private home, and no sudo privileges."
fi
printf '%s\n' "Switch with sudo -iu gitrunr, then run scripts/pi-setup.sh from a readable checkout."
