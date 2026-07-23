#!/bin/bash

# Copyright (C) 2026 Red Hat, Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.

set -eu

daemon=$1
test_directory=$(mktemp -d /tmp/plymouthd-executable-test-XXXXXX)
trap 'rm -rf "$test_directory"' EXIT

help_output=$test_directory/help.out
help_error=$test_directory/help.err

echo 'TAP version 13'
echo '1..1'

if ! "$daemon" --help > "$help_output" 2> "$help_error"; then
        echo 'not ok 1 - daemon help exits successfully'
        exit 1
fi

expected_options=(
        --help
        --attach-to-session
        --no-daemon
        --debug
        --debug-file
        --mode
        --pid-file
        --kernel-command-line
        --tty
        --no-boot-log
        --ignore-serial-consoles
        --graphical-boot
)

for option in "${expected_options[@]}"; do
        if ! grep --quiet --fixed-strings -- "$option" "$help_output"; then
                echo "not ok 1 - daemon help omits $option"
                exit 1
        fi
done

expected_modes='Mode is one of: boot-up, shutdown, reboot, updates, system-upgrade, firmware-upgrade, system-reset'
if [[ -s $help_error ]] ||
        ! grep --quiet --fixed-strings "$expected_modes" "$help_output"; then
        echo 'not ok 1 - daemon help describes its complete option surface'
        exit 1
fi

echo 'ok 1 - daemon help describes its complete option surface'
