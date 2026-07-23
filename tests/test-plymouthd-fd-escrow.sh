#!/bin/bash

# Copyright (C) 2026 Red Hat, Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.

set -eu

escrow=$1
escrow_pid=

cleanup()
{
        if [[ -n $escrow_pid ]] && kill -0 "$escrow_pid" 2> /dev/null; then
                kill -KILL "$escrow_pid" 2> /dev/null || true
        fi
        if [[ -n $escrow_pid ]]; then
                wait "$escrow_pid" 2> /dev/null || true
        fi
}
trap cleanup EXIT

"$escrow" &
escrow_pid=$!

echo 'TAP version 13'
echo '1..1'

process_name=
for attempt in {1..100}; do
        if [[ -r /proc/$escrow_pid/cmdline ]]; then
                process_name=$(tr '\0' '\n' < /proc/$escrow_pid/cmdline |
                        sed -n '1p')
                if [[ $process_name == @* ]]; then
                        break
                fi
        fi
        sleep 0.01
done

if [[ $process_name != @* ]]; then
        echo "not ok 1 - escrow marks process name: got '$process_name'"
        exit 1
fi

echo 'ok 1 - escrow marks process name for shutdown survival'
