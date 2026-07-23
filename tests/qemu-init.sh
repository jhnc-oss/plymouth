#!/usr/bin/bash

# Copyright (C) 2026 Red Hat, Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.

set -u

export LANG=C
export LC_ALL=C
export PATH=/usr/sbin:/usr/bin

serial_device=/dev/ttyS0
debug_log=/run/plymouth/debug.log

report()
{
        printf '%s\n' "$*" > "$serial_device"
}

show_debug_log()
{
        if [[ -r $debug_log ]]; then
                report "--- plymouth debug log ---"
                /usr/bin/cat "$debug_log" > "$serial_device"
                report "--- end plymouth debug log ---"
        fi
}

finish()
{
        local status=$1

        printf 'b' > /proc/sysrq-trigger
        exit "$status"
}

fail()
{
        report "PLYMOUTH_QEMU_FAIL: $*"
        show_debug_log
        finish 1
}

/usr/bin/mount -t devtmpfs devtmpfs /dev ||
        fail "could not mount devtmpfs"
/usr/bin/mount -t proc proc /proc ||
        fail "could not mount proc"
/usr/bin/mount -t sysfs sysfs /sys ||
        fail "could not mount sysfs"

/usr/sbin/plymouthd \
        --no-daemon \
        --no-boot-log \
        --tty=/dev/tty1 \
        --pid-file=/run/plymouth/pid \
        --kernel-command-line="splash plymouth.ignore-serial-consoles" \
        --debug \
        --debug-file="$debug_log" &
daemon_pid=$!

daemon_ready=false
for ((attempt = 0; attempt < 50; attempt++)); do
        if /usr/bin/plymouth --ping > /dev/null 2>&1; then
                daemon_ready=true
                break
        fi

        if ! kill -0 "$daemon_pid" 2> /dev/null; then
                wait "$daemon_pid"
                fail "daemon exited before accepting client connections"
        fi

        /usr/bin/sleep 0.1
done

[[ $daemon_ready == true ]] ||
        fail "daemon did not accept client connections"

/usr/bin/plymouth --has-active-vt ||
        fail "daemon did not acquire the boot virtual terminal"
/usr/bin/plymouth show-splash ||
        fail "show-splash request failed"
/usr/bin/plymouth update --status="QEMU initramfs boot" ||
        fail "status update failed"
/usr/bin/plymouth display-message --text="Plymouth boot test" ||
        fail "display-message request failed"
/usr/bin/plymouth hide-message --text="Plymouth boot test" ||
        fail "hide-message request failed"
/usr/bin/plymouth hide-splash ||
        fail "hide-splash request failed"
/usr/bin/plymouth show-splash ||
        fail "second show-splash request failed"
/usr/bin/plymouth quit ||
        fail "quit request failed"

wait "$daemon_pid"
daemon_status=$?
if ((daemon_status != 0)); then
        fail "daemon exited with status $daemon_status"
fi

report "PLYMOUTH_QEMU_PASS"
finish 0
