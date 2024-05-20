#!/usr/bin/bash
/coastnet/build/debug/classify_coastnet --class=41 --aspect-ratio=4 --network-filename=/coastnet/models/coastnet-surface.pt < $3 > $4
source /venv/bin/activate
python /usr/local/etc/combiner.py $3 $4