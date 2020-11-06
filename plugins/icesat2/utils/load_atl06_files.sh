RESOURCE_FILE=$1
BUCKET=icesat2-sliderule
THREADS=4

cat $RESOURCE_FILE | parallel -j$THREADS hsload -v --link s3://$BUCKET/data/ATL06/{} /hsds/ATL06/
