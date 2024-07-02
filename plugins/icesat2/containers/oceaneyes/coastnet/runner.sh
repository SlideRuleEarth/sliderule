#!/usr/bin/bash
/coastnet/build/release/classify --verbose --model-filename=/coastnet/models/coastnet_model-20240628.json < $3 > $4
/env/bin/python /coastnet/trimmer.py $4