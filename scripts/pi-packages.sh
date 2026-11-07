#!/usr/bin/env bash
# Shared package list for administrator provisioning and runner validation.
pi_packages=(build-essential cmake git curl jq ca-certificates util-linux libicu-dev libssl-dev libkrb5-3 zlib1g liblttng-ust-dev libunwind8)

pi_missing_packages() {
    missing=()
    local package
    for package in "${pi_packages[@]}"; do
        if [[ $(dpkg-query -W -f='${db:Status-Status}' "$package" 2>/dev/null || true) != installed ]]; then
            missing+=("$package")
        fi
    done
}
