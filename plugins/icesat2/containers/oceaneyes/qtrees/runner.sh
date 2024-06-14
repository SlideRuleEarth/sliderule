#!/usr/bin/bash
/qtrees/build/release/classify --verbose --model-filename=/qtrees/models/model-20240607.json < $3 > $4
/env/bin/python /qtrees/combiner.py $3 $4