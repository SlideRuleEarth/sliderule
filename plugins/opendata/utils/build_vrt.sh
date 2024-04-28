#!/bin/bash

# Check if two parameters are passed
if [ "$#" -ne 2 ]; then
  echo "Usage: $0 <vrt_output_file> <input_file_list>"
  exit 1
fi

# Assign parameters
vrt_output_file="$1"
input_file_list="$2"

# Set GDAL environment variables
# CPL_CURL_VERBOSE=ON \
# CPL_DEBUG=ON \
GDAL_MAX_CACHE=1024 \
GDAL_NUM_THREADS=ALL_CPUS \
GDAL_DISABLE_READDIR_ON_OPEN=EMPTY_DIR \
AWS_NO_SIGN_REQUEST=YES \

# Run gdalbuildvrt with the provided parameters
gdalbuildvrt "$vrt_output_file" -input_file_list "$input_file_list"