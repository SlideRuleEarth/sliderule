Steps to generates VRT files for opnedata plugins
Open Data on AWS host data but does not provide stac end point, vrt, etc.
We build our own VRT files and host them in SlideRule s3 bucket.


**********************************************
*** ESA WorldCover 2021 product.
**********************************************

1- Run aws ls command on the bucket where map.tif files are located for 2021

 aws s3 --no-sign-request ls  s3://esa-worldcover/v200/2021/map/ > map_list.txt

2- Edit map_list.txt and remove/replace entries, the map_list.txt should have entries in a form:

https://esa-worldcover.s3.amazonaws.com/v200/2021/map/ESA_WorldCover_10m_2021_v200_N00E006_Map.tif
https://esa-worldcover.s3.amazonaws.com/v200/2021/map/ESA_WorldCover_10m_2021_v200_N00E009_Map.tif
.
.

3- Run:
./build_vrt.sh ESA_WorldCover_10m_2021_v200_Map_local.vrt map_list.txt

NOTE: building of the vrt takes several hours.

ESA_WorldCover_10m_2021_v200_Map_local.vrt is our vrt but it has https urls in it.
We need to convert https urls to vsis3 paths.

4- Run sed command:

sed 's,https://esa-worldcover.s3.amazonaws.com,/vsis3/esa-worldcover,g' ESA_WorldCover_10m_2021_v200_Map_local.vrt >  ESA_WorldCover_10m_2021_v200_Map.vrt

5- Verify that ESA_WorldCover_10m_2021_v200_Map.vrt has correct /vsis3 paths to the tifs.
6- Put the vrt into our s3 bucket:  /vsis3/sliderule/data/WORLDCOVER/ESA_WorldCover_10m_2021_v200_Map.vrt
7- Updated asset_directory.csv



**********************************************
*** Meta's Global Canopy Height product.
**********************************************

https://registry.opendata.aws/dataforgood-fb-forests/
https://dataforgood.facebook.com/dfg/tools/canopy-height-maps#accessdata

1- Create map_list.txt
aws s3 ls --no-sign-request s3://dataforgood-fb-data/forests/v1/alsgedi_global_v6_float/chm/ > map_list.txt

2- Cleanup map_list.txt it should have one .tif file on each line
001311332.tif
001311333.tif
.
.

3- Prepend vsis3 path to the files/paths in map_list.txt by running command:

 sed 's/^/\/vsis3\/dataforgood-fb-data\/forests\/v1\/alsgedi_global_v6_float\/chm\//' map_list.txt > vsis3_map_list.txt

Paths in vsis3_map_list.txt should look like this:
/vsis3/dataforgood-fb-data/forests/v1/alsgedi_global_v6_float/chm/313112003.tif

3- Run:
./build_vrt.sh META_GlobalCanopyHeight_1m_2024_v1.vrt vsis3_map_list.txt

NOTE: building of the vrt takes several hours.

4- Verify vrt
5- Put the vrt into our s3 bucket:  /vsis3/sliderule/data/GLOBALCANOPY/META_GlobalCanopyHeight_1m_2024_v1.vrt
6- Updated asset_directory.csv

