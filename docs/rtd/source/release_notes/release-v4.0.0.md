# Release v4.0.0

2023-09-26

Version description of the v4.0.0 release of ICESat-2 SlideRule.  This document also captures functionality added in versions v3.5.0, v3.6.0, and v3.7.0.

## Breaking Changes

This version contains a number of backward-incompatible changes, specifically to the names of the fields being returned by the `atl03s` and `atl06` APIs.  These changes were made to standardize the downstream processing of the photon and elevation data, and also to bring the names of the fields being returned by SlideRule closer to the ICESat-2 Standard Data Products.

For `atl03s` and `atl03sp` APIs:
* `distance` is now `x_atc`
* Across track distance is now `y_atc`

For `atl06` and `atl06p` APIs:
* `distance` is now `x_atc`
* Across track distance is now `y_atc`
* `dh_fit_dy` has been removed
* `lon` is now `longitude`
* `lat` is now `latitude`

The compact ATL06 record `atl06rec-compact` has been removed entirely and is no longer supported.

## Major Changes

* Added new capability to subset rasters. The `subsets` endpoint subsets rasters and returns the data back to the user.  It works in conjunction with the `raster.subset` Python API. 

* Added new `opendata` plugin that supports sampling the ESA World Cover 10m dataset.

* The Python client supports returning a GeoDataFrame with 3D point geometry.  The user must supply a "height" column name in the API call in order to receive back a GeoDataFrame with the 3D point gemetry; note there is a performance impact when selected. (See [#272](https://github.com/ICESat2-SlideRule/sliderule/pull/272)).

* User's can supply their own PROJ pipeline in raster sampling requests. To use this feature, add a "proj_pipeline" string to your request dictionary:
```Python
proj_pipeline = "+proj=pipeline +step +proj=axisswap +order=2,1 +step +proj=unitconvert +xy_in=deg +xy_out=rad +step +proj=stere +lat_0=-90 +lat_ts=-71 +lon_0=70 +x_0=6000000 +y_0=6000000 +ellps=WGS84"
parms = { "poly": region['poly'],
          "cnf": "atl03_high",
          "ats": 5.0,
          "cnt": 5,
          "len": 20.0,
          "res": 10.0,
          "maxi": 1,
          "samples": {"mosaic": {"asset": "rema-mosaic", "radius": 10.0, "zonal_stats": True, "proj_pipeline": proj_pipeline}} }
gdf = icesat2.atl06p(parms, asset=asset, resources=[resource])
```

* Localization of the CRS transform when GDAL is trying to select the best transform to use (did I say that right???).  If what I said is confusing, please reach out to Eric.  For non-mosiac datasets (i.e. arcticdem-strips, rema-strips, 3dep, landsat), the code will automatically provide a bounding box of the raster being sampled to GDAL when it does the coordinate transformation.  For mosaic datasets, you can now provide your own bounding box with the request.  To do so, add the "aoi_bbox" parameter to your request dictionary:
```Python
region = sliderule.toregion('grandmesa.geojson')
bbox = raster.poly2bbox(region["poly"])
parms = { "poly": region['poly'],
          "cnf": "atl03_high",
          "ats": 5.0,
          "cnt": 5,
          "len": 20.0,
          "res": 10.0,
          "maxi": 1,
          "samples": {"gedi": {"asset": "gedil3-elevation", "aoi_bbox": bbox}} 
}
```

* Overhaul of how ancillary fields are handled by the server-side code.  A new system-wide record object type has been added called a "container record".  A container record holds inside of it other records that are grouped and processed together.  Ancillary data associated with an ATL03 extent or ATL06 elevation is now placed in a container record along with the corresponding ATL03 or ATL06 record.

* Parquet file generation has been optimized and the gzip compression has been replaced with snappy compression.

* Default ICESat-2 products used now set to version 006.

* The ATL03 subsetting code now operates on one spot per thread (it used to be one pair track per thread).  This change is made in conjunction with a change in the base AWS instance type to an instance that has 8 cores.  The additional subsetting threads will therefore be able to utilize the greater number of cores when subsetting and speed up processing requests when only one subsetting operation on a given node is being processed.

* When making raster sample requests, the user no longer needs to perform a separate earthdata.* call to generate a catalog for the 3DEP and Landsat raster datasets, but the client is not smart enouh to know to generate the catalog on its own if the user does not supply it.

## Development Updates

* SlideRule Python bindings (srpy) have been updated to support building them inside a conda managed OS package environment.

* The Python bindings for H5Coro are superceded by the new pure Python implementation effort: [h5coro](https://github.com/ICESat2-SlideRule/h5coro)

* Major rewrite of GDAL raster sampling code: new class structure has GdalRaster as a stand-alone class, with RasterObject parenting a GeoRaster and a GeoIndexedRaster for mosaic and strip datasets respectively.

## Issues Resolved

* [#283](https://github.com/ICESat2-SlideRule/sliderule/issues/283) - feature request: report y_atc in ATL06-SR

* [#233](https://github.com/ICESat2-SlideRule/sliderule/issues/233) - Create performance PyTests

* [#235](https://github.com/ICESat2-SlideRule/sliderule/issues/235) - Need PyTests for raster sampling of strips

* [#246](https://github.com/ICESat2-SlideRule/sliderule/issues/246) - Add PyTest for file_directory raster code

* [#262](https://github.com/ICESat2-SlideRule/sliderule/issues/262) - Replace "nodata" value with NaN as sampled value in GeoRaster

* [#268](https://github.com/ICESat2-SlideRule/sliderule/issues/268) - Mismatch in pyH5Coro declarations

* [#280](https://github.com/ICESat2-SlideRule/sliderule/issues/280) - Make default number of ATL06 iterations 6

* [#284](https://github.com/ICESat2-SlideRule/sliderule/issues/284) - Feature idea : extract arbitrary fields from ATL03 to ATL06-SR

* [#293](https://github.com/ICESat2-SlideRule/sliderule/issues/293) - Add ESA WorldCover plugin

* [#299](https://github.com/ICESat2-SlideRule/sliderule/issues/299) - Errors reading ArcticDEM mosaic

* [#309](https://github.com/ICESat2-SlideRule/sliderule/issues/309) - GeoUserRaster causes use after free

* [#312](https://github.com/ICESat2-SlideRule/sliderule/issues/312) - Bitmask/flags rasters should only be read when flags param is set

## Getting This Release

[https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v4.0.0](https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v4.0.0)

[https://github.com/ICESat2-SlideRule/sliderule-icesat2/releases/tag/v4.0.0](https://github.com/ICESat2-SlideRule/sliderule-icesat2/releases/tag/v4.0.0)

[https://github.com/ICESat2-SlideRule/sliderule-python/releases/tag/v4.0.0](https://github.com/ICESat2-SlideRule/sliderule-python/releases/tag/v4.0.0)

