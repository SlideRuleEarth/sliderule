GEBCO Data Processing Guide

The GEBCO data is not hosted on AWS. Users must download the compressed files and run the provided scripts to create COGs and a GeoJSON index file.
Below are the steps to perform this process. In these examples, the year "2024" is used, but the Python scripts support the "year" parameter for other years as well.

*** Create Directory Structure ***
Create the following directories with the specified year in the path:

/data/GEBCO/2024/TIFS
/data/GEBCO/2024/COGS

*** Download the GEBCO Data ***
Go to: https://www.gebco.net/data_and_products/gridded_bathymetry_data/#a1
Find the table entry "GEBCO_2024 Grid (sub-ice topo/bathy)" download DataGeoTiff
It should be named something like: gebco_2024_sub_ice_topo_geotiff.zip

Find the table entry "GEBCO_2024 TID Grid" download DataGeoTiff
It should be named something like: gebco_2024_tid_geotiff.zip

Uncompress both files into: /data/GEBCO/2024/TIFS
>> unzip gebco_2024_sub_ice_topo_geotiff.zip -d /data/GEBCO/2024/TIFS
>> unzip gebco_2024_tid_geotiff.zip -d /data/GEBCO/2024/TIFS


*** Run the Python Scripts ***
From the utils directory where the scripts are located, run the following scripts in the specified order.
Make sure to include the year as a parameter.

Step 1: Run the script cog_maker.py with the year as a parameter
>> python cog_maker.py 2024

Step 2: Run the script index_maker.py with the year as a parameter
>> python index_maker.py 2024


*** Upload to Sliderule S3 Bucket ***
In the Sliderule public bucket (s3://sliderule/data/GEBCO/), create a new directory for the year, e.g., 2024.

Upload all COG tif files and index.geojso from /data/GEBCO/2024/COGS to: s3://sliderule/data/GEBCO/2024
>> aws s3 cp . s3://sliderule/data/GEBCO/2024/ --recursive



Upload the original downloaded data files to: s3://sliderule/data/GEBCO/2024
>> aws s3 cp gebco_2024_sub_ice_topo_geotiff.zip s3://sliderule/data/GEBCO/2024/gebco_2024_sub_ice_topo_geotiff.zip
>> aws s3 cp gebco_2024_tid_geotiff.zip s3://sliderule/data/GEBCO/2024/gebco_2024_tid_geotiff.zip

