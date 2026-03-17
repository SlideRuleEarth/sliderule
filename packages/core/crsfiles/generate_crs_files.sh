#!/bin/bash
# Generate CRS PROJJSON files

set -e  # exit on error

# Find this script’s directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
UTIL_PATH="$SCRIPT_DIR/../../../clients/python/utils/crs2projjson.py"

set -e  # exit on error

# Find this script’s directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
UTIL_PATH="$SCRIPT_DIR/../../../clients/python/utils/crs2projjson.py"

# ITRF2014
EPSG_CODE=7912
python3 "$UTIL_PATH" EPSG:${EPSG_CODE} --outfile EPSG${EPSG_CODE}.projjson
echo "Generated: EPSG${EPSG_CODE}.projjson"

python3 "$UTIL_PATH" EPSG:${EPSG_CODE} EGM08 --outfile EPSG${EPSG_CODE}_EGM08.projjson
echo "Generated: EPSG${EPSG_CODE}_EGM08.projjson"

# ITRF2020
EPSG_CODE=9989
python3 "$UTIL_PATH" EPSG:${EPSG_CODE} --outfile EPSG${EPSG_CODE}.projjson
echo "Generated: EPSG${EPSG_CODE}.projjson"

python3 "$UTIL_PATH" EPSG:${EPSG_CODE} EGM08 --outfile EPSG${EPSG_CODE}_EGM08.projjson
echo "Generated: EPSG${EPSG_CODE}_EGM08.projjson"

# python3 "$UTIL_PATH" EPSG:${EPSG_CODE} NAVD88 --outfile EPSG${EPSG_CODE}_NAVD88.projjson
# echo "Generated: EPSG${EPSG_CODE}_NAVD88.projjson"

echo "All CRS files generated successfully."
