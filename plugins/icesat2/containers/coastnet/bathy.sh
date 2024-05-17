#!/usr/bin/bash
/coastnet/build/debug/classify_coastnet --class=40 --aspect-ratio=4 --network-filename=$2 < $1 > $3
