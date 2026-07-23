#!/bin/bash

# Copyright (C) 2026 Red Hat, Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.

set -eu

client=$1
test_directory=$(mktemp -d /tmp/plymouth-executable-test-XXXXXX)
trap 'rm -rf "$test_directory"' EXIT

help_output=$test_directory/help.out
help_error=$test_directory/help.err

echo 'TAP version 13'
echo '1..1'

if ! "$client" --help > "$help_output" 2> "$help_error"; then
        echo 'not ok 1 - client help exits successfully'
        exit 1
fi

expected_options=(
        --help
        --debug
        --get-splash-plugin-path
        --newroot
        --quit
        --ping
        --has-active-vt
        --sysinit
        --show-splash
        --hide-splash
        --ask-for-password
        --ignore-keystroke
        --update
        --details
        --wait
)

expected_commands=(
        change-mode
        system-update
        update-root-fs
        ask-question
        display-message
        hide-message
        watch-keystroke
        pause-progress
        unpause-progress
        report-error
        deactivate
        reactivate
        reload
)

for option in "${expected_options[@]}"; do
        if ! grep --quiet --fixed-strings -- "$option" "$help_output"; then
                echo "not ok 1 - client help omits $option"
                exit 1
        fi
done

for command in "${expected_commands[@]}"; do
        if ! grep --quiet --fixed-strings -- "$command" "$help_output"; then
                echo "not ok 1 - client help omits $command"
                exit 1
        fi
done

if [[ -s $help_error ]]; then
        echo 'not ok 1 - client help writes to standard error'
        exit 1
fi

echo 'ok 1 - client help describes options and commands'
