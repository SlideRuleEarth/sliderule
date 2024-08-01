#!/usr/bin/bash
/openoceans/build/release/classify --verbose < $3 > $4
/env/bin/python /openoceans/combiner.py $3 $4