#!/bin/bash

# Copyright (C) 2026 Red Hat, Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.

set -eu

theme_chooser=$1
test_directory=$(mktemp -d /tmp/plymouth-set-theme-XXXXXX)
trap 'rm -rf "$test_directory"' EXIT

data_directory=$test_directory/share
config_directory=$test_directory/etc
policy_directory=$test_directory/policy
plugin_directory=$test_directory/plugins
libexec_directory=$test_directory/libexec
command_directory=$test_directory/bin

mkdir -p "$data_directory/plymouth/themes/alpha"
mkdir -p "$data_directory/plymouth/themes/zeta"
mkdir -p "$plugin_directory" "$command_directory"
printf '%s\n' '[Plymouth Theme]' 'ModuleName=script' > \
        "$data_directory/plymouth/themes/alpha/alpha.plymouth"
printf '%s\n' '[Plymouth Theme]' 'ModuleName=script' > \
        "$data_directory/plymouth/themes/zeta/zeta.plymouth"

run_theme_chooser()
{
        PLYMOUTH_DATADIR=$data_directory \
        PLYMOUTH_CONFDIR=$config_directory \
        PLYMOUTH_POLICYDIR=$policy_directory \
        PLYMOUTH_PLUGIN_PATH=$plugin_directory/ \
        PLYMOUTH_LIBEXECDIR=$libexec_directory \
        PATH=$command_directory:$PATH \
                "$theme_chooser" "$@"
}

echo 'TAP version 13'
echo '1..3'

themes=$(run_theme_chooser --list)
if [[ $themes == $'alpha\nzeta' ]]; then
        echo 'ok 1 - list installed themes'
else
        echo "not ok 1 - list installed themes: got '$themes'"
        exit 1
fi

printf '%s\n' '#!/bin/sh' 'echo 0' > "$command_directory/id"
chmod +x "$command_directory/id"
: > "$plugin_directory/script.so"
run_theme_chooser alpha

if grep --quiet --line-regexp 'Theme=alpha' \
        "$config_directory/plymouthd.conf"; then
        echo 'ok 2 - select installed theme'
else
        echo 'not ok 2 - select installed theme'
        exit 1
fi

printf '%s\n' 'Keep=1' >> "$config_directory/plymouthd.conf"
run_theme_chooser --reset

if ! grep --quiet '^Theme=' "$config_directory/plymouthd.conf" &&
        grep --quiet --line-regexp 'Keep=1' \
                "$config_directory/plymouthd.conf"; then
        echo 'ok 3 - reset configured theme'
else
        echo 'not ok 3 - reset configured theme'
        exit 1
fi
