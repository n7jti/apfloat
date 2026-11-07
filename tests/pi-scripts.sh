#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)
tmp=$(mktemp -d)
trap 'rm -rf -- "$tmp"' EXIT
mkdir -p "$tmp/bin"

cat > "$tmp/bin/uname" <<'EOF'
#!/usr/bin/env bash
case "$1" in
    -s) echo Linux ;;
    -m) echo "${MOCK_ARCH:-aarch64}" ;;
esac
EOF
cat > "$tmp/bin/id" <<'EOF'
#!/usr/bin/env bash
case "$1" in
    -u) echo "${MOCK_UID:-1000}" ;;
    -G) echo "${MOCK_GROUPS:-1000}" ;;
esac
EOF
cat > "$tmp/bin/df" <<'EOF'
#!/usr/bin/env bash
printf 'Filesystem 1024-blocks Used Available Capacity Mounted\nfake 3000000 1 2000000 1%% /\n'
EOF
cat > "$tmp/bin/dpkg-query" <<'EOF'
#!/usr/bin/env bash
if [[ ${MOCK_MISSING_PACKAGE:-} == "${!#}" && ! -e $MOCK_PACKAGES_INSTALLED ]]; then exit 1; fi
printf installed
EOF
cat > "$tmp/bin/sudo" <<'EOF'
#!/usr/bin/env bash
printf '%s\n' "$*" >> "$MOCK_LOG"
if [[ $1 == -n ]]; then
    if [[ ${MOCK_SUDO_ALLOWED:-0} == 1 ]]; then echo "allowed"; exit 0; fi
    echo "User gitrunr is not allowed to run sudo"
    exit 1
fi
exit "${MOCK_SUDO_STATUS:-0}"
EOF
cat > "$tmp/bin/getent" <<'EOF'
#!/usr/bin/env bash
if [[ $1 == passwd && -e $MOCK_ACCOUNT ]]; then
    echo "gitrunr:x:${MOCK_ACCOUNT_UID:-1000}:1000::$MOCK_ACCOUNT_HOME:/bin/bash"
elif [[ $1 == group && ${MOCK_GROUP_CONFLICT:-0} == 1 ]]; then
    echo "gitrunr:x:1000:"
else
    exit 2
fi
EOF
cat > "$tmp/bin/useradd" <<'EOF'
#!/usr/bin/env bash
[[ $* == "-m -U -s /bin/bash -p ! gitrunr" ]] || exit 1
mkdir -p "$MOCK_ACCOUNT_HOME"
touch "$MOCK_ACCOUNT"
EOF
cat > "$tmp/bin/passwd" <<'EOF'
#!/usr/bin/env bash
echo "gitrunr ${MOCK_PASSWORD_STATUS:-L} 01/01/2020 0 99999 7 -1"
EOF
cat > "$tmp/bin/stat" <<'EOF'
#!/usr/bin/env bash
if [[ ${!#} == "$MOCK_ACCOUNT_HOME" ]]; then echo "1000:700"; else exec /usr/bin/stat "$@"; fi
EOF
cat > "$tmp/bin/apt-get" <<'EOF'
#!/usr/bin/env bash
echo "$*" >> "$MOCK_LOG"
[[ ${MOCK_APT_FAIL:-0} == 0 ]] || exit 7
touch "$MOCK_PACKAGES_INSTALLED"
EOF
cat > "$tmp/bin/cmake" <<'EOF'
#!/usr/bin/env bash
echo "cmake version ${MOCK_CMAKE_VERSION:-3.25.0}"
EOF
cat > "$tmp/bin/c++" <<'EOF'
#!/usr/bin/env bash
exit "${MOCK_COMPILER_STATUS:-0}"
EOF
cat > "$tmp/bin/curl" <<'EOF'
#!/usr/bin/env bash
[[ ${MOCK_NETWORK_FAIL:-0} == 0 ]] || exit 22
while (($#)); do
    if [[ $1 == -o ]]; then
        if [[ $2 == *.tar.gz && ${MOCK_CHECKSUM_FAIL:-0} == 1 ]]; then
            printf 'corrupted' > "$2"
        else
            printf 'fixture' > "$2"
        fi
        exit
    fi
    shift
done
exit 1
EOF
cat > "$tmp/bin/jq" <<'EOF'
#!/usr/bin/env bash
if [[ $* == *tag_name* ]]; then echo v2.1.0; exit; fi
[[ ${MOCK_MISSING_DIGEST:-0} == 0 ]] || exit 1
echo 'sha256:f16d05ec6b29248d2c61adb1e9263f78e4f7bace1b955014a2d17872cfe4064d'
EOF
cat > "$tmp/bin/tar" <<'EOF'
#!/usr/bin/env bash
while (($#)); do
    if [[ $1 == -C ]]; then dest=$2; break; fi
    shift
done
cat > "$dest/config.sh" <<'CONFIG'
#!/usr/bin/env bash
[[ "$*" == '--labels raspberry-pi' ]] || exit 4
[[ ${MOCK_REGISTER_FAIL:-0} == 0 ]] || exit 5
touch .runner .credentials
CONFIG
cat > "$dest/run.sh" <<'RUN'
#!/usr/bin/env bash
echo foreground-runner
if [[ ${MOCK_WAIT:-0} == 1 ]]; then
    trap 'exit 143' TERM
    printf '%s\n' "$$" > "$MOCK_PID_FILE"
    while :; do sleep 0.1; done
fi
exit "${MOCK_RUN_STATUS:-0}"
RUN
chmod +x "$dest/config.sh" "$dest/run.sh"
EOF
chmod +x "$tmp/bin/"*
export PATH="$tmp/bin:$PATH"
export MOCK_LOG="$tmp/sudo.log"
export MOCK_ACCOUNT="$tmp/account" MOCK_ACCOUNT_HOME="$tmp/account-home" MOCK_PACKAGES_INSTALLED="$tmp/packages-installed"
mkdir -p "$tmp/admin-scripts"
sed "s|/home/gitrunr|$MOCK_ACCOUNT_HOME|g" "$root/scripts/pi-create-user.sh" > "$tmp/admin-scripts/pi-create-user.sh"
cp "$root/scripts/pi-packages.sh" "$tmp/admin-scripts/pi-packages.sh"
chmod +x "$tmp/admin-scripts/pi-create-user.sh"
mkdir -p "$tmp/home"
export HOME="$tmp/home"

run() {
    local expected=$1 command=$2
    shift 2
    local status=0
    script -q -e -c "bash '$root/scripts/$command'" /dev/null > "$tmp/output" 2>&1 || status=$?
    if [[ $status != "$expected" ]]; then
        printf 'Expected status %s, got %s for %s\n' "$expected" "$status" "$command" >&2
        cat "$tmp/output" >&2
        exit 1
    fi
    for text in "$@"; do grep -Fq "$text" "$tmp/output" || { cat "$tmp/output" >&2; exit 1; }; done
}

run_admin() {
    local expected=$1 text=$2 status=0
    script -q -e -c "bash '$tmp/admin-scripts/pi-create-user.sh'" /dev/null > "$tmp/output" 2>&1 || status=$?
    if [[ $status != "$expected" ]] || { [[ -n $text ]] && ! grep -Fq "$text" "$tmp/output"; }; then
        printf 'Account test failed: expected %s and %s, got %s\n' "$expected" "$text" "$status" >&2
        cat "$tmp/output" >&2
        exit 1
    fi
}

export MOCK_SUDO_STATUS=7
run_admin 7 ''
unset MOCK_SUDO_STATUS
export MOCK_UID=0 MOCK_GROUP_CONFLICT=1
run_admin 1 'group or home already exists'
unset MOCK_GROUP_CONFLICT
run_admin 0 'Created gitrunr'
run_admin 0 'Existing gitrunr account verified'
export MOCK_SUDO_ALLOWED=1
run_admin 1 'Could not verify'
unset MOCK_SUDO_ALLOWED
export MOCK_GROUPS='1000 27'
run_admin 1 'supplementary groups'
unset MOCK_GROUPS
export MOCK_PASSWORD_STATUS=P
run_admin 1 'password is not locked'
unset MOCK_PASSWORD_STATUS
export MOCK_ACCOUNT_UID=0
run_admin 1 'Account properties are unexpected'
unset MOCK_ACCOUNT_UID
mkdir -p "$MOCK_ACCOUNT_HOME/.ssh"
touch "$MOCK_ACCOUNT_HOME/.ssh/authorized_keys"
run_admin 1 'SSH authorized keys'
rm "$MOCK_ACCOUNT_HOME/.ssh/authorized_keys"
unset MOCK_UID
export MOCK_UID=0 MOCK_MISSING_PACKAGE=git MOCK_APT_FAIL=1
run_admin 7 'Installing missing packages'
unset MOCK_APT_FAIL
run_admin 0 'Installing missing packages'
grep -Fq 'install -y git' "$MOCK_LOG"
rm "$MOCK_PACKAGES_INSTALLED"
unset MOCK_UID MOCK_MISSING_PACKAGE

export MOCK_ARCH=x86_64
run 1 pi-setup.sh '64-bit ARM Linux is required'
unset MOCK_ARCH
export MOCK_CMAKE_VERSION=3.19.0
run 1 pi-setup.sh 'CMake 3.20 or newer'
unset MOCK_CMAKE_VERSION
export MOCK_COMPILER_STATUS=1
run 1 pi-setup.sh 'cannot compile a C++17 program'
unset MOCK_COMPILER_STATUS
export MOCK_MISSING_DIGEST=1
run 1 pi-setup.sh 'digest is missing'
unset MOCK_MISSING_DIGEST
export MOCK_CHECKSUM_FAIL=1
run 1 pi-setup.sh 'checksum mismatch'
unset MOCK_CHECKSUM_FAIL
export MOCK_NETWORK_FAIL=1
run 22 pi-setup.sh
unset MOCK_NETWORK_FAIL
export MOCK_MISSING_PACKAGE=git
run 1 pi-setup.sh 'Missing packages: git'
unset MOCK_MISSING_PACKAGE
run 0 pi-setup.sh 'Runner registered'
run 0 pi-setup.sh 'Existing runner found'
chmod 644 "$HOME/.local/share/apfloat/actions-runner/.credentials"
run 1 pi-setup.sh 'credentials are accessible'
run 1 pi-session.sh 'credentials are accessible'
chmod 600 "$HOME/.local/share/apfloat/actions-runner/.credentials"
export MOCK_RUN_STATUS=42
run 42 pi-session.sh 'foreground-runner'
unset MOCK_RUN_STATUS
export MOCK_WAIT=1 MOCK_PID_FILE="$tmp/runner.pid"
script -q -e -c "bash '$root/scripts/pi-session.sh'" /dev/null > "$tmp/output" 2>&1 &
session_pid=$!
for ((i = 0; i < 100; ++i)); do
    [[ -f $MOCK_PID_FILE ]] && break
    sleep 0.1
done
if [[ ! -f $MOCK_PID_FILE ]]; then
    kill "$session_pid"
    wait "$session_pid" || true
    echo "Runner did not start" >&2
    exit 1
fi
kill -TERM "$(< "$MOCK_PID_FILE")"
status=0
wait "$session_pid" || status=$?
[[ $status == 143 ]] || { echo "Runner termination was not propagated: $status" >&2; exit 1; }
unset MOCK_WAIT MOCK_PID_FILE
rm "$HOME/.local/share/apfloat/actions-runner/.credentials"
run 1 pi-setup.sh 'incomplete'
run 1 pi-session.sh 'not registered'
mkdir "$tmp/second-home"
export HOME="$tmp/second-home"
export MOCK_REGISTER_FAIL=1
run 5 pi-setup.sh
unset MOCK_REGISTER_FAIL
run 1 pi-setup.sh 'incomplete'
printf '%s\n' 'Pi script mock tests passed.'
