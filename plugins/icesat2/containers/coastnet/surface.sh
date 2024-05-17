#!/usr/bin/bash
/coastnet/build/debug/classify_coastnet --class=41 --aspect-ratio=4 --network-filename=$2 < $1 > $3
