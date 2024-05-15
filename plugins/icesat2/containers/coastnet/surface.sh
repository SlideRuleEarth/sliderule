#!/usr/bin/bash

# Bash strict mode
set -euo pipefail
IFS=$'\n\t'

# Use debug build unless overridden from cmdline
: ${build:=debug}

echo build = ${build}

find $1 | \
    parallel --verbose --lb --jobs=15 \
        "/ut-coastnet/build/${build}/classify_coastnet \
        --class=41 \
        --aspect-ratio=4 \
        --network-filename=$2 \
        < {} > $3"
