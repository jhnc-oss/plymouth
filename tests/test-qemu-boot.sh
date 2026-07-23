#!/usr/bin/bash

# Copyright (C) 2026 Red Hat, Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.

set -euo pipefail

if (($# < 1 || $# > 2)); then
        echo "usage: $0 BUILD_DIRECTORY [KERNEL_IMAGE]" >&2
        exit 2
fi

source_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
build_directory=$(realpath -e "$1")
kernel_image=${2:-}

if [[ -z $kernel_image ]]; then
        mapfile -t kernel_candidates < <(
                find /usr/lib/modules \
                        -mindepth 2 \
                        -maxdepth 2 \
                        -type f \
                        -name vmlinuz \
                        -print |
                        sort -V
        )

        if ((${#kernel_candidates[@]} == 0)); then
                echo "no kernel image found under /usr/lib/modules" >&2
                exit 1
        fi

        kernel_image=${kernel_candidates[-1]}
fi

kernel_image=$(realpath -e "$kernel_image")

for command in \
        awk \
        cpio \
        cp \
        find \
        grep \
        install \
        ldd \
        ln \
        mkdir \
        mktemp \
        qemu-system-x86_64 \
        realpath \
        rm \
        sort \
        tee \
        timeout; do
        if ! command -v "$command" > /dev/null; then
                echo "required command is unavailable: $command" >&2
                exit 1
        fi
done

dracut_install=/usr/lib/dracut/dracut-install
if [[ ! -x $dracut_install ]]; then
        echo "required dependency installer is unavailable: $dracut_install" >&2
        exit 1
fi

test_directory=$(mktemp -d /tmp/plymouth-qemu-boot-XXXXXX)
initramfs_root=$test_directory/root
initramfs_image=$test_directory/initramfs.cpio
qemu_log=$test_directory/qemu.log

cleanup()
{
        rm -rf -- "$test_directory"
}
trap cleanup EXIT

mkdir -p \
        "$initramfs_root/dev" \
        "$initramfs_root/etc/plymouth" \
        "$initramfs_root/proc" \
        "$initramfs_root/run/plymouth" \
        "$initramfs_root/sys" \
        "$initramfs_root/tmp" \
        "$initramfs_root/usr/bin" \
        "$initramfs_root/usr/lib64/plymouth" \
        "$initramfs_root/usr/lib64/plymouth/renderers" \
        "$initramfs_root/usr/sbin" \
        "$initramfs_root/usr/share/X11" \
        "$initramfs_root/usr/share/plymouth/themes/details" \
        "$initramfs_root/var/lib/plymouth" \
        "$initramfs_root/var/log"

ln -s usr/bin "$initramfs_root/bin"
ln -s usr/lib "$initramfs_root/lib"
ln -s usr/lib64 "$initramfs_root/lib64"
ln -s usr/sbin "$initramfs_root/sbin"

artifact_sources=(
        "$build_directory/src/plymouthd"
        "$build_directory/src/client/plymouth"
        "$build_directory/src/libply/libply.so.5.0.0"
        "$build_directory/src/libply-splash-core/libply-splash-core.so.5.0.0"
        "$build_directory/src/libply-splash-graphics/libply-splash-graphics.so.5.0.0"
        "$build_directory/src/plugins/splash/details/details.so"
        "$build_directory/src/plugins/renderers/frame-buffer/frame-buffer.so"
)
artifact_destinations=(
        /usr/sbin/plymouthd
        /usr/bin/plymouth
        /usr/lib64/libply.so.5.0.0
        /usr/lib64/libply-splash-core.so.5.0.0
        /usr/lib64/libply-splash-graphics.so.5.0.0
        /usr/lib64/plymouth/details.so
        /usr/lib64/plymouth/renderers/frame-buffer.so
)

for ((index = 0; index < ${#artifact_sources[@]}; index++)); do
        source=${artifact_sources[index]}
        destination=${artifact_destinations[index]}

        if [[ ! -f $source ]]; then
                echo "required build artifact is unavailable: $source" >&2
                exit 1
        fi

        install -D -m 0755 "$source" "$initramfs_root$destination"
done

ln -s libply.so.5.0.0 "$initramfs_root/usr/lib64/libply.so.5"
ln -s libply-splash-core.so.5.0.0 \
        "$initramfs_root/usr/lib64/libply-splash-core.so.5"
ln -s libply-splash-graphics.so.5.0.0 \
        "$initramfs_root/usr/lib64/libply-splash-graphics.so.5"

install -D -m 0755 "$source_root/tests/qemu-init.sh" "$initramfs_root/init"
install -D -m 0644 \
        "$source_root/tests/qemu-plymouthd.conf" \
        "$initramfs_root/etc/plymouth/plymouthd.conf"
install -D -m 0644 \
        "$source_root/themes/details/details.plymouth" \
        "$initramfs_root/usr/share/plymouth/themes/details/details.plymouth"
install -D -m 0644 \
        "$source_root/src/plymouthd.defaults" \
        "$initramfs_root/usr/share/plymouth/plymouthd.defaults"

if [[ -e /etc/os-release ]]; then
        install -D -m 0644 \
                "$(realpath -e /etc/os-release)" \
                "$initramfs_root/etc/os-release"
fi

if [[ -d /usr/share/X11/xkb ]]; then
        cp -aL \
                /usr/share/X11/xkb \
                "$initramfs_root/usr/share/X11/xkb"
fi

project_library_path=$build_directory/src/libply
project_library_path+=:$build_directory/src/libply-splash-core
project_library_path+=:$build_directory/src/libply-splash-graphics

declare -A host_library_set=()
for artifact in "${artifact_sources[@]}"; do
        dependencies=$(LD_LIBRARY_PATH=$project_library_path ldd "$artifact")
        if grep -Fq "not found" <<< "$dependencies"; then
                echo "unresolved dependency for $artifact:" >&2
                echo "$dependencies" >&2
                exit 1
        fi

        while IFS= read -r library; do
                if [[ $library != "$build_directory"/* ]]; then
                        host_library_set["$library"]=1
                fi
        done < <(
                awk \
                        '$2 == "=>" && $3 ~ /^\// { print $3 }
                         $1 ~ /^\// { print $1 }' \
                        <<< "$dependencies"
        )
done

host_programs=(
        "$(command -v bash)"
        "$(command -v cat)"
        "$(command -v mkdir)"
        "$(command -v mount)"
        "$(command -v sleep)"
)

"$dracut_install" \
        --destrootdir "$initramfs_root" \
        --ldd \
        --all \
        "${host_programs[@]}" \
        "${!host_library_set[@]}"

(
        cd "$initramfs_root"
        find . -print0 |
                sort -z |
                cpio --null --create --format=newc --quiet
) > "$initramfs_image"

set +e
timeout --foreground 90s \
        qemu-system-x86_64 \
        -accel tcg \
        -cpu max \
        -m 256M \
        -smp 1 \
        -kernel "$kernel_image" \
        -initrd "$initramfs_image" \
        -append "console=ttyS0,115200 rdinit=/init panic=1 oops=panic quiet loglevel=3" \
        -display none \
        -monitor none \
        -serial stdio \
        -nic none \
        -no-reboot \
        2>&1 |
        tee "$qemu_log"
qemu_status=${PIPESTATUS[0]}
set -e

if grep -Fq "PLYMOUTH_QEMU_FAIL" "$qemu_log"; then
        echo "Plymouth reported a failure inside the QEMU guest" >&2
        exit 1
fi

if ! grep -Fq "PLYMOUTH_QEMU_PASS" "$qemu_log"; then
        echo "QEMU guest did not report a successful Plymouth boot test" >&2
        exit 1
fi

if ((qemu_status != 0)); then
        echo "QEMU exited with status $qemu_status after the guest test" >&2
        exit 1
fi

echo "QEMU initramfs boot test passed"
