#!/bin/bash

# Copyright (C) 2026 Red Hat, Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.

set -eu

generator=$1
test_directory=$(mktemp -d /tmp/plymouth-generate-version-XXXXXX)
trap 'rm -rf "$test_directory"' EXIT
export TZ=UTC

create_repository()
{
        repository=$test_directory/$1
        commit_date=$2

        mkdir -p "$repository/scripts"
        cp "$generator" "$repository/scripts/generate-version.sh"
        git -C "$repository" init --quiet
        git -C "$repository" config user.email tests@plymouth.invalid
        git -C "$repository" config user.name 'Plymouth Tests'
        printf '%s\n' fixture > "$repository/tracked-file"
        git -C "$repository" add tracked-file
        GIT_AUTHOR_DATE=$commit_date GIT_COMMITTER_DATE=$commit_date \
                git -C "$repository" commit --quiet --message initial
}

echo 'TAP version 13'
echo '1..3'

create_repository release-tag 2024-01-02T00:00:00Z
git -C "$repository" tag 24.10
version=$("$repository/scripts/generate-version.sh")

if [[ $version == 24.10 ]]; then
        echo 'ok 1 - exact release tag'
else
        echo "not ok 1 - exact release tag: got '$version'"
        exit 1
fi

create_repository development-version 2024-01-02T12:00:00Z
version=$("$repository/scripts/generate-version.sh")

if [[ $version == 24.002.1 ]]; then
        echo 'ok 2 - development date and commit count'
else
        echo "not ok 2 - development date and commit count: got '$version'"
        exit 1
fi

create_repository release-version-floor 2024-01-01T12:00:00Z
git -C "$repository" tag 99.9.5
printf '%s\n' second >> "$repository/tracked-file"
git -C "$repository" add tracked-file
GIT_AUTHOR_DATE=2024-01-02T12:00:00Z \
GIT_COMMITTER_DATE=2024-01-02T12:00:00Z \
        git -C "$repository" commit --quiet --message second
version=$("$repository/scripts/generate-version.sh")

if [[ $version == 99.9.6 ]]; then
        echo 'ok 3 - release version floor'
else
        echo "not ok 3 - release version floor: got '$version'"
        exit 1
fi
