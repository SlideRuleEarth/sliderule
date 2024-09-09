To run a specific algorithm in oceaneyes:

`docker run -it --rm -v /data:/data --name oceaneyes 742127912612.dkr.ecr.us-west-2.amazonaws.com/oceaneyes:latest /env/bin/python /<algorithm>/runner.py {\"spot\":<spot>} <path_to_granule_json> <path_to_granule> <path_to_output>`

> where _spot_ is from [1, 2, 3, 4, 5, 6]
>
> and _algorithm_ is from [cshelph, medianfilter, openoceans, pointnet, bathypathfinder]

For example, to execute cshelph on spot 3 of granule ATL24_20181027185143_04450108_005_01.parquet:

```bash
docker run -it --rm -v /data:/data --name oceaneyes 742127912612.dkr.ecr.us-west-2.amazonaws.com/oceaneyes:latest /env/bin/python /cshelph/runner.py {\"spot\":3} /data/ATL24/ATL24_20181027185143_04450108_005_01.parquet.json /data/ATL24/ATL24_20181027185143_04450108_005_01.parquet /data/ATL24/ATL24_20181027185143_04450108_005_01_cshelph_3.csv
```