SRC=$1
DST=$2
h5repack -v -l CONTI $SRC $DST.tmp
h5repack -v -f GZIP=6 $DST.tmp $DST
rm $DST.tmp
