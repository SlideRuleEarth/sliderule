#!/usr/bin/bash
/coastnet/build/release/classify_coastnet --class=40 --aspect-ratio=4 --network-filename=/coastnet/models/coastnet-bathy.pt < $3 > $4
source /venv/bin/activate
python /coastnet/trimmer.py $4