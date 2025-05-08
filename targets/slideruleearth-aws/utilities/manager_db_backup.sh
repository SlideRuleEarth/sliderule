ORG="${1:-sliderule}"
NOW=$(date "+%Y%m%d")
aws s3 cp s3://sliderule/config/manager-$ORG.db s3://sliderule/data/backup/manager-$ORG-$NOW.db