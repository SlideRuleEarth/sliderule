#!/usr/bin/bash -ve
root=$(dirname $0)
conda run -n sliderule_env python -W ignore $root/lpdaac_download.py -f $1 -dir .
aws s3 cp $1 s3://sliderule/data/GEDI/
rm $1