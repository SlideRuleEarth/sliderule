#!/usr/bin/sh
export LOKI_ADDR=http://localhost:3100
export PATH=/usr/local/bin:$PATH
export PATH=/usr/bin:$PATH
clustername=`cat /clustername.txt` 
logfile=$clustername$(date +'_%Y_%m_%d_log.txt')
fromdate=$(date -d "yesterday" '+%Y-%m-%d')T00:00:00Z
logcli query -o jsonl --limit 1000000 --batch 5000 --from=${fromdate} '{job="sliderule",level=~"debug|info|warning|error|critical"}' > ${logfile} || exit 1
gzip -9 ${logfile}
aws s3 cp ${logfile}.gz s3://sliderule/logs/ || exit 1
rm ${logfile}.gz