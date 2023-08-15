#!/bin/bash

# CPL_CURL_VERBOSE=ON \
# CPL_DEBUG=ON \
GDAL_DISABLE_READDIR_ON_OPEN=EMPTY_DIR \
AWS_NO_SIGN_REQUEST=YES \
gdalbuildvrt ESA_WorldCover_10m_2021_v200_Map_local.vrt -input_file_list map_list.txt
