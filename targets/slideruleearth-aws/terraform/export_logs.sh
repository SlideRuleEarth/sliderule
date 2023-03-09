#!/usr/bin/sh
export LOKI_ADDR=http://localhost:3100
export PATH=/usr/local/bin:$PATH
export PATH=/usr/bin:$PATH
clustername=`cat /clustername.txt` 
logfile=$clustername$(date +'_%Y_%m_%d_log.txt')
logcli query -o raw --limit 500000000 '{job="sliderule"}' > ${logfile} || exit 1
gzip -9 ${logfile}
aws s3 cp ${logfile}.gz s3://sliderule/logs/ || exit 1
rm ${logfile}.gz