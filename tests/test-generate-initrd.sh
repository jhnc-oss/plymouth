#!/bin/bash

# Copyright (C) 2026 Red Hat, Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.

set -eu

generate_initrd=$1
test_directory=$(mktemp -d /tmp/plymouth-generate-initrd-XXXXXX)
trap 'rm -rf "$test_directory"' EXIT
unset DESTDIR

populate_command=$test_directory/populate-initrd
image_file=$test_directory/initrd-plymouth.img

printf '%s\n' \
        '#!/bin/bash' \
        'set -eu' \
        'while [[ $# -gt 0 ]]; do' \
        '        case $1 in' \
        '                -t|--targetdir)' \
        '                        shift' \
        '                        target_directory=$1' \
        '                        ;;' \
        '        esac' \
        '        shift' \
        'done' \
        'mkdir -p "$target_directory/etc" "$target_directory/usr/bin" "$target_directory/lib64"' \
        'printf "%s\n" "[Daemon]" "Theme=alpha" > "$target_directory/etc/plymouthd.conf"' \
        'printf "%s\n" client > "$target_directory/usr/bin/plymouth"' \
        'printf "%s\n" host-library > "$target_directory/lib64/libc.so.6"' \
        'printf "%s\n" plymouth-library > "$target_directory/lib64/libplymouth-private.so"' > \
        "$populate_command"
chmod +x "$populate_command"

echo 'TAP version 13'
echo '1..2'

PLYMOUTH_POPULATE_INITRD=$populate_command \
PLYMOUTH_IMAGE_FILE=$image_file \
        "$generate_initrd"

archive_contents=$(gzip --decompress --stdout "$image_file" |
        cpio --list 2>/dev/null)

if grep --quiet --line-regexp 'etc/plymouthd.conf' <<< "$archive_contents" &&
        grep --quiet --line-regexp 'usr/bin/plymouth' <<< "$archive_contents"; then
        echo 'ok 1 - generated cpio contents'
else
        echo 'not ok 1 - generated cpio contents'
        exit 1
fi

if ! grep --quiet --line-regexp 'lib64/libc.so.6' <<< "$archive_contents" &&
        grep --quiet --line-regexp 'lib64/libplymouth-private.so' \
                <<< "$archive_contents"; then
        echo 'ok 2 - omit host-provided shared libraries'
else
        echo 'not ok 2 - omit host-provided shared libraries'
        exit 1
fi
