aws configure set default.s3.multipart_threshold 10GB
aws s3 sync s3://sliderule-public/atl24r3/h5/ s3://sliderule/data/ATL24r3/ --copy-props default --metadata-directive COPY
aws s3 sync s3://sliderule-public/atl24r3/xml/ s3://sliderule/data/ATL24r3/ --copy-props default --metadata-directive COPY