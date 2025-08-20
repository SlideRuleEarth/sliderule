#!/bin/bash
# Generate CRS PROJJSON files

set -e  # exit on error

# Find this script’s directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
UTIL_PATH="$SCRIPT_DIR/../../../clients/python/utils/crs2projjson.py"

echo "Generating EPSG7912.projjson ..."
python3 "$UTIL_PATH" EPSG:7912 --outfile EPSG7912.projjson
echo "Done: EPSG7912.projjson"

echo "Generating EPSG7912_EGM08.projjson ..."
python3 "$UTIL_PATH" EPSG:7912 EGM08 --outfile EPSG7912_EGM08.projjson
echo "Done: EPSG7912_EGM08.projjson"

echo "Generating EPSG7912_NAVD88.projjson ..."
python3 "$UTIL_PATH" EPSG:7912 NAVD88 --outfile EPSG7912_NAVD88.projjson
echo "Done: EPSG7912_NAVD88.projjson"

echo "All CRS files generated successfully."
