# Purpose: rechunk ATL03 files in parallel
# Example: ./rechunk_atl03_dir.sh /data/ATLAS /data/ATL03

SRC_DIR=$1
DST_DIR=$2
THREADS=4

ls $SRC_DIR | parallel -j$THREADS ./rechunk_atl03_file.sh $SRC_DIR/{} $DST_DIR/{}
