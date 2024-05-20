#!/usr/bin/bash
/coastnet/build/debug/classify_coastnet --class=40 --aspect-ratio=4 --network-filename=/coastnet/models/coastnet-bathy.pt < $3 > $4
