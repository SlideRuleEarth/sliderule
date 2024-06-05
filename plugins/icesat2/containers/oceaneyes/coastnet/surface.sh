#!/usr/bin/bash
/coastnet/build/release/classify_coastnet --verbose --class=41 --aspect-ratio=4 --network-filename=/coastnet/models/coastnet-surface.pt < $3 > $4
/env/bin/python /coastnet/combiner.py $3 $4