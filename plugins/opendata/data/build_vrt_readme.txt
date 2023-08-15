Steps to generate vrt file for WorldCover 2021 product.

1- Run aws ls command on the bucket where map.tif files are located for 2021

 aws s3 --no-sign-request ls  s3://esa-worldcover/v200/2021/map/ > map_list.txt

2- Edit map_list.txt and remove/replace entries, the map_list.txt should have entries in a form:

https://esa-worldcover.s3.amazonaws.com/v200/2021/map/ESA_WorldCover_10m_2021_v200_N00E006_Map.tif
https://esa-worldcover.s3.amazonaws.com/v200/2021/map/ESA_WorldCover_10m_2021_v200_N00E009_Map.tif
.
.
.

3- Run ./build_vrt.sh

NOTE: building of the vrt may take several hours.

ESA_WorldCover_10m_2021_v200_Map_local.vrt is our vrt but it has https urls in it.
We need to convert https urls to vsis3 paths.

4- Run sed command:

sed 's,https://esa-worldcover.s3.amazonaws.com,/vsis3/esa-worldcover,g' ESA_WorldCover_10m_2021_v200_Map_local.vrt >  ESA_WorldCover_10m_2021_v200_Map.vrt

5- Verify that ESA_WorldCover_10m_2021_v200_Map.vrt has correct /vsis3 paths to the tifs.