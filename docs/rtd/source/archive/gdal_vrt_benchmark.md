# VRT Performance Benchmarking

2022-11-10

## Overview

Test reads elevation value from ArcticDem. POI is lon: -74.60  lat: 82.86  
Method used vrt file created from mosaic rasters, version 3.0, 2m, hosted on AWS.  
The mosaic.vrt file is stored locally on aws dev server at /data/ArcticDem/mosaic.vrt  

The actuall raster containing the elevation for POI is:   
/vsis3/pgc-opendata-dems/arcticdem/mosaics/v3.0/2m/34_37/34_37_1_1_2m_v3.0_reg_dem.tif  

The elevation can be read from the aws file directly:  
gdallocationinfo -wgs84 /vsis3/pgc-opendata-dems/arcticdem/mosaics/v3.0/2m/34_37/34_37_1_1_2m_v3.0_reg_dem.tif -valonly -74.6 82.86  

The elevation can be read from the mosaic.vrt on our dev server:  
gdallocationinfo -wgs84 /data/ArcticDem/mosaic.vrt -valonly -74.6 82.86  


The test reads the same POI one million times. Gdal library should access the proper raster tile from S3 bucket and use it locally 
for all remining elevation reads.   

In the first implementation the mosaic.vrt is opened, the vrtdataset and vrtband are used to do a direct read via:  
vrtband->RasterIO(GF_Read, col, row, 1, 1, &elevation, 1, 1, GDT_Float32, 0, 0, 0);  
col and row are calculated for the mosaic.vrt  

This aproach works correctly but performance is very poor. It took almost 170 seconds to do one million reads. Each 100k reads takes almost 17 seconds.  
  
Points read:	100000	16988.167999778  
Points read:	200000	16991.582000162  
Points read:	300000	16994.273000164  
Points read:	400000	16995.032000123  
Points read:	500000	16998.428999912  
Points read:	600000	16988.674999913  
Points read:	700000	16993.327999953  
Points read:	800000	17000.902000116  
Points read:	900000	16991.788999876  
Points read:	1000000	16998.240999877  
  
1000000	points read time: 169940.77899982 msecs  
  
  
In the second aproach the vrtdataset and vrtband are used ONLY to find the name of the raster containing POI.  
The code is very similar to how gdallocationinfo tool is implemented using xml parsing in GDAL.  
The raster tif file is than opened and this raster's dataset and band are used to do a read using col, row calculated for that
raster. This method is much more efficient. It only took 665 msecs to do one million reads.  
  
Points read:	100000	66.570000024512  
Points read:	200000	66.518999869004  
Points read:	300000	66.537000006065  
Points read:	400000	66.584999905899  
Points read:	500000	66.590999951586  
Points read:	600000	66.584999905899  
Points read:	700000	66.558999940753  
Points read:	800000	66.634999820963  
Points read:	900000	66.579000093043  
Points read:	1000000	66.580000100657  
  
1000000	points read time: 665.83800013177 msecs  
  

The test was also reapeated with POI beigng different each time read is executed but always within the same raster file. This caused 
more S3 transactions for GDAL to read many tiles. The performance degraded where the second 'fast' method took almost 2 seconds to read a million POIs which the first method took almost the same amount of time.   



