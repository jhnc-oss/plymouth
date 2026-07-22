#!/bin/bash

# Copyright (C) 2026 Red Hat, Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.

set -eu

populate_initrd=$1
test_directory=$(mktemp -d /tmp/plymouth-populate-initrd-XXXXXX)
trap 'rm -rf "$test_directory"' EXIT
unset DESTDIR

sysroot=$test_directory/sysroot
command_directory=$test_directory/bin
initrd_directory=$test_directory/initrd
override_initrd_directory=$test_directory/initrd-override

mkdir -p "$command_directory"
mkdir -p "$sysroot/usr/bin" "$sysroot/usr/sbin"
mkdir -p "$sysroot/usr/libexec/plymouth"
mkdir -p "$sysroot/usr/lib/plymouth/renderers"
mkdir -p "$sysroot/usr/share/plymouth/themes/alpha"
mkdir -p "$sysroot/usr/share/plymouth/themes/text"
mkdir -p "$sysroot/usr/share/plymouth/themes/details"
mkdir -p "$sysroot/custom-theme"
mkdir -p "$sysroot/etc/plymouth"

printf '%s\n' binary > "$sysroot/usr/sbin/plymouthd"
printf '%s\n' binary > "$sysroot/usr/bin/plymouth"
printf '%s\n' binary > \
        "$sysroot/usr/libexec/plymouth/plymouthd-fd-escrow"
chmod +x "$sysroot/usr/sbin/plymouthd" "$sysroot/usr/bin/plymouth"
chmod +x "$sysroot/usr/libexec/plymouth/plymouthd-fd-escrow"

: > "$sysroot/usr/lib/libfixture.so"
: > "$sysroot/usr/lib/plymouth/text.so"
: > "$sysroot/usr/lib/plymouth/details.so"
: > "$sysroot/usr/lib/plymouth/script.so"
: > "$sysroot/usr/lib/plymouth/renderers/frame-buffer.so"
: > "$sysroot/usr/share/plymouth/logo.png"
printf '%s\n' fixture > "$sysroot/etc/system-release"

printf '%s\n' '[Daemon]' 'Theme=alpha' > \
        "$sysroot/etc/plymouth/plymouthd.conf"
printf '%s\n' '[Daemon]' 'Theme=text' > \
        "$sysroot/usr/share/plymouth/plymouthd.defaults"
printf '%s\n' '[Plymouth Theme]' 'ModuleName=text' > \
        "$sysroot/usr/share/plymouth/themes/text/text.plymouth"
printf '%s\n' '[Plymouth Theme]' 'ModuleName=details' > \
        "$sysroot/usr/share/plymouth/themes/details/details.plymouth"
printf '%s\n' \
        '[Plymouth Theme]' \
        'ModuleName=script' \
        'ImageDir=/usr/share/plymouth/themes/alpha' > \
        "$sysroot/usr/share/plymouth/themes/alpha/alpha.plymouth"
printf '%s\n' image > \
        "$sysroot/usr/share/plymouth/themes/alpha/background.png"
printf '%s\n' '[Plymouth Theme]' 'ModuleName=script' > \
        "$sysroot/custom-theme/alpha.plymouth"
printf '%s\n' custom-image > "$sysroot/custom-theme/custom.png"

printf '%s\n' '#!/bin/sh' \
        'echo "libfixture.so => /usr/lib/libfixture.so (0x1)"' > \
        "$command_directory/fixture-ldd"
printf '%s\n' '#!/bin/sh' 'echo alpha' > \
        "$command_directory/plymouth-set-default-theme"
printf '%s\n' '#!/bin/sh' 'exit 0' > "$command_directory/fc-match"
chmod +x "$command_directory/fixture-ldd"
chmod +x "$command_directory/plymouth-set-default-theme"
chmod +x "$command_directory/fc-match"

run_populate_initrd()
{
        PATH=$command_directory:$PATH \
        PLYMOUTH_SYSROOT=$sysroot \
        PLYMOUTH_LDD=$command_directory/fixture-ldd \
        PLYMOUTH_LDD_PATH=$command_directory:$PATH \
        PLYMOUTH_LIBEXECDIR=/usr/libexec \
        PLYMOUTH_DATADIR=/usr/share \
        PLYMOUTH_PLUGIN_PATH=/usr/lib/plymouth \
        PLYMOUTH_LOGO_FILE=/usr/share/plymouth/logo.png \
        PLYMOUTH_CONFDIR=/etc/plymouth \
        PLYMOUTH_POLICYDIR=/usr/share/plymouth \
        PLYMOUTH_DAEMON_PATH=/usr/sbin/plymouthd \
        PLYMOUTH_CLIENT_PATH=/usr/bin/plymouth \
        PLYMOUTH_DRM_ESCROW_PATH=/usr/libexec/plymouth/plymouthd-fd-escrow \
        SYSTEMD_UNIT_DIR=/not-present \
                "$populate_initrd" \
                --targetdir "$1" \
                --x11-directory /not-present
}

echo 'TAP version 13'
echo '1..2'

run_populate_initrd "$initrd_directory"

required_paths=(
        /usr/sbin/plymouthd
        /usr/bin/plymouth
        /usr/libexec/plymouth/plymouthd-fd-escrow
        /usr/lib/libfixture.so
        /usr/lib/plymouth/script.so
        /usr/lib/plymouth/renderers/frame-buffer.so
        /usr/share/plymouth/themes/alpha/alpha.plymouth
        /usr/share/plymouth/themes/alpha/background.png
)

for required_path in "${required_paths[@]}"; do
        if [[ ! -f $initrd_directory$required_path ]]; then
                echo "not ok 1 - populate dependencies: missing $required_path"
                exit 1
        fi
done

echo 'ok 1 - populate dependencies'

PLYMOUTH_THEME_NAME=alpha \
PLYMOUTH_CONFIGURED_DIR_PATH=/custom-theme \
        run_populate_initrd "$override_initrd_directory"

override_config=$override_initrd_directory/etc/plymouth/plymouthd.conf
if grep --quiet --line-regexp 'Theme=alpha' "$override_config" &&
        grep --quiet --line-regexp 'ThemeDir=/custom-theme' "$override_config" &&
        [[ -f $override_initrd_directory/custom-theme/custom.png ]]; then
        echo 'ok 2 - rewrite overridden theme configuration'
else
        echo 'not ok 2 - rewrite overridden theme configuration'
        exit 1
fi
