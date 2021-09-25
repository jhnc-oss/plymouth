#!/bin/sh 

cd "$(dirname "$0")" || exit
autoreconf --install --symlink -Wno-portability
if [ -z "$NOCONFIGURE" ]; then
    exec ./configure "$@"
fi
