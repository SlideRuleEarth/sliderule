#!/usr/bin/bash -ve
conda run -n sliderule_env python -W ignore lpdaac_download.py -f $1 -dir .
aws s3 cp $1 s3://sliderule/data/GEDI/
rm $1