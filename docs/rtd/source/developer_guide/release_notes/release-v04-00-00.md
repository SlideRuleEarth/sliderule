# Release v4.0.x

2023-11-01

Version description of the v4.0.4 release of ICESat-2 SlideRule.

## Breaking Changes

This version contains a number of backward-incompatible changes, specifically to the names of the fields being returned by the `atl03s` and `atl06` APIs, and the Python client function APIs.  These changes were made to standardize the downstream processing of the photon and elevation data, and also to bring the names of the fields being returned by SlideRule closer to the ICESat-2 Standard Data Products.

* **v4.0.0** - For `atl03s` and `atl03sp` APIs:
  * `distance` is now `x_atc`
  * Across track distance is now `y_atc`

* **v4.0.0** - For `atl06` and `atl06p` APIs:
  * `distance` is now `x_atc`
  * Across track distance is now `y_atc`
  * `dh_fit_dy` has been removed
  * `lon` is now `longitude`
  * `lat` is now `latitude`

* **v4.0.0** - The compact ATL06 record `atl06rec-compact` has been removed entirely and is no longer supported.

* **v4.0.0** - The `asset` parameter has been removed from the Python client functions and is now managed internally by the client.  Users can still override the asset by supplying an `asset` key in the request parameters; but going forward users should not generally need to worry about which assets the servers are using.

## Major Changes

* **v4.0.0** - Added new capability to subset rasters. The `subsets` endpoint subsets rasters and returns the data back to the user.  It works in conjunction with the `raster.subset` Python API.

* **v4.0.0** - Added new `opendata` plugin that supports sampling the ESA World Cover 10m dataset.

* **v4.0.0** - The Python client supports returning a GeoDataFrame with 3D point geometry.  The user must supply a "height" column name in the API call in order to receive back a GeoDataFrame with the 3D point geometry; note there is a performance impact when selected. (See [#272](https://github.com/SlideRuleEarth/sliderule/pull/272)).

* **v4.0.0** - User's can supply their own PROJ pipeline in raster sampling requests. To use this feature, add a "proj_pipeline" string to your request dictionary:
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

* **v4.0.0** - Localization of the CRS transform when GDAL is trying to select the best transform to use (did I say that right???).  If what I said is confusing, please reach out to Eric.  For non-mosiac datasets (i.e. arcticdem-strips, rema-strips, 3dep, landsat), the code will automatically provide a bounding box of the raster being sampled to GDAL when it does the coordinate transformation.  For mosaic datasets, you can now provide your own bounding box with the request.  To do so, add the "aoi_bbox" parameter to your request dictionary:
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

* **v4.0.0** - Overhaul of how ancillary fields are handled by the server-side code.  A new system-wide record object type has been added called a "container record".  A container record holds inside of it other records that are grouped and processed together.  Ancillary data associated with an ATL03 extent or ATL06 elevation is now placed in a container record along with the corresponding ATL03 or ATL06 record.

* **v4.0.0** - Parquet file generation has been optimized and the gzip compression has been replaced with snappy compression.  Also the ability to generate a non-geo parquet file has been fixed, where the latitude and longitude are present as normal columns and there is no geometry column.

* **v4.0.0** - Default ICESat-2 products used now set to version 006.

* **v4.0.0** - The ATL03 subsetting code now operates on one spot per thread (it used to be one pair track per thread).  This change is made in conjunction with a change in the base AWS instance type to an instance that has 8 cores.  The additional subsetting threads will therefore be able to utilize the greater number of cores when subsetting and speed up processing requests when only one subsetting operation on a given node is being processed.

* **v4.0.0** - When making raster sample requests, the user no longer needs to perform a separate earthdata.* call to generate a catalog for the 3DEP and Landsat raster datasets, but the client is not smart enough to know to generate the catalog on its own if the user does not supply it.

## Development Updates

* **v4.0.4** - [#338](https://github.com/SlideRuleEarth/sliderule/pull/338), [#339](https://github.com/SlideRuleEarth/sliderule/pull/339) - Codespell pre-commit hook added to sliderule repository; along with it a slew of spelling corrections.

* **v4.0.4** - [#350](https://github.com/SlideRuleEarth/sliderule/pull/350) - major update to code base to support running clang-tidy, cppcheck, and address sanitizer on all Debug builds and selftests.

* **v4.0.4** - [75c7f7d](https://github.com/SlideRuleEarth/sliderule/commit/75c7f7d5e0dfb324d3fa5cd51a53e2a375c1a76f) - Initialization function is now a fixture in the pytests

* **v4.0.4** - [#208](https://github.com/SlideRuleEarth/sliderule/issues/208) - Certificate for slideruleearth.io automatically rotated using certbot and AWS lambda

* **v4.0.4** - [#251](https://github.com/SlideRuleEarth/sliderule/issues/251) - version information added to documentation

* **v4.0.4** - [29de90f](https://github.com/SlideRuleEarth/sliderule/commit/29de90fc2d44561519fed63cbf2ecc8dadb11bed) - metric collection moved to orchestrator

* **v4.0.0** - SlideRule Python bindings (srpy) have been updated to support building them inside a conda managed OS package environment.

* **v4.0.0** - The Python bindings for H5Coro are superseded by the new pure Python implementation effort: [h5coro](https://github.com/SlideRuleEarth/h5coro)

* **v4.0.0** - Major rewrite of GDAL raster sampling code: new class structure has GdalRaster as a stand-alone class, with RasterObject parenting a GeoRaster and a GeoIndexedRaster for mosaic and strip datasets respectively.

* **v4.0.0** - The `latest` tagged docker images for the sliderule compute node, the intelligent load balancer, the proxy, and the monitor, are now all built automatically on every push to `main` by a "Build and Deploy" GitHub Action workflow.  The new images are used when the automated Pytests are kicked off for the `developers` cluster.

## Issues Resolved

* **v4.0.4** - [f173932](https://github.com/SlideRuleEarth/sliderule/commit/f173932f98d85cb888fc8c921f69b15512acdfb5) - updates to support latest ipywidget release

* **v4.0.4** - [dd0fdcd](https://github.com/SlideRuleEarth/sliderule/commit/dd0fdcdca69740068798c7c420dcd07906f71528) - debug message logged when request to ILB fails

* **v4.0.4** - [3b535c5](https://github.com/SlideRuleEarth/sliderule/commit/3b535c588f9fa3c54e0f4ab86ea223c5f8463c23) - fix ATL06-SR bug when processing longitudes outside (-150, 150)

* **v4.0.4** - [#342](https://github.com/SlideRuleEarth/sliderule/issues/342) - fixed broken links in documentation

* **v4.0.4** - [1c41114](https://github.com/SlideRuleEarth/sliderule/commit/1c411140d211ba33658d8503a63f24536d198391) - default all raster datasets to us-west-2, unless overridden; saves a few cURL calls undernearth the hood

* **v4.0.4** - [#345](https://github.com/SlideRuleEarth/sliderule/pull/345) - updated soft limits for number of open files to support large raster sampling requests

* **v4.0.4** - [d2d9b63](https://github.com/SlideRuleEarth/sliderule/commit/d2d9b637cdeec2b041e7736e4e6d9af9794fa776) - GDAL multi-threaded support added for VRTs

* **v4.0.4** - [264433b](https://github.com/SlideRuleEarth/sliderule/commit/264433b980998c5db6fe9ee2fc672ba436d8da52) - fixed bug in ATL06-SR where invalid memory was freed when a segment containing no photons was processed

* **v4.0.4** - [5bbfaa6](https://github.com/SlideRuleEarth/sliderule/commit/5bbfaa6cf6aeee6b071c529b993afb458f6c5af6) - security vulnerability found by GitHub in node.js client fixed

* **v4.0.4** - [d1e776b](https://github.com/SlideRuleEarth/sliderule/commit/d1e776b5fa0922c196b8230369d6b97af1cfd85f) - GeoJSONRaster optimized to faster subsetting of ATL03 via burned geojson

* **v4.0.4** - [61fd6db](https://github.com/SlideRuleEarth/sliderule/commit/61fd6dbc5fcd3299f19790d72177f67a1b03b85f) - social login to provisioning system from the Python client is now supported

* **v4.0.4** - CurlLib replaced HttpClient in Orchestrator and EndpointProxy, significantly improving performance

* **v4.0.4** - [35671ed](https://github.com/SlideRuleEarth/sliderule/commit/35671ed32349fc8360bd8b4f8590b11dfc8974e5) - updated GEDI L4B data product directory entry to match latest release

* **v4.0.0** - [#182](https://github.com/SlideRuleEarth/sliderule/issues/182) - Across track slope not calculated for ATL06

* **v4.0.0** - [#202](https://github.com/SlideRuleEarth/sliderule/issues/202) - Feature request: export x_atc and y_atc

* **v4.0.0** - [#226](https://github.com/SlideRuleEarth/sliderule/issues/226) - Migrate ps-web documentation to provisioning system

* **v4.0.0** - [#233](https://github.com/SlideRuleEarth/sliderule/issues/233) - Create performance PyTests

* **v4.0.0** - [#235](https://github.com/SlideRuleEarth/sliderule/issues/235) - Need PyTests for raster sampling of strips

* **v4.0.0** - [#246](https://github.com/SlideRuleEarth/sliderule/issues/246) - Add PyTest for file_directory raster code

* **v4.0.0** - [#262](https://github.com/SlideRuleEarth/sliderule/issues/262) - Replace "nodata" value with NaN as sampled value in GeoRaster

* **v4.0.0** - [#268](https://github.com/SlideRuleEarth/sliderule/issues/268) - Mismatch in pyH5Coro declarations

* **v4.0.0** - [#270](https://github.com/SlideRuleEarth/sliderule/issues/270) - Using H5Coro in stand alone mode

* **v4.0.0** - [#280](https://github.com/SlideRuleEarth/sliderule/issues/280) - Make default number of ATL06 iterations 6

* **v4.0.0** - [#283](https://github.com/SlideRuleEarth/sliderule/issues/283) - feature request: report y_atc in ATL06-SR

* **v4.0.0** - [#284](https://github.com/SlideRuleEarth/sliderule/issues/284) - Feature idea : extract arbitrary fields from ATL03 to ATL06-SR

* **v4.0.0** - [#293](https://github.com/SlideRuleEarth/sliderule/issues/293) - Add ESA WorldCover plugin

* **v4.0.0** - [#295](https://github.com/SlideRuleEarth/sliderule/issues/295) - Requests to TNM for 3DEP 1m is timing out

* **v4.0.0** - [#299](https://github.com/SlideRuleEarth/sliderule/issues/299) - Errors reading ArcticDEM mosaic

* **v4.0.0** - [#300](https://github.com/SlideRuleEarth/sliderule/issues/300) - Put querying the catalog for raster datasets inside the API functions in the client

* **v4.0.0** - [#309](https://github.com/SlideRuleEarth/sliderule/issues/309) - GeoUserRaster causes use after free

* **v4.0.0** - [#312](https://github.com/SlideRuleEarth/sliderule/issues/312) - Bitmask/flags rasters should only be read when flags param is set

## Getting This Release

[https://github.com/SlideRuleEarth/sliderule/releases/tag/v4.0.4](https://github.com/SlideRuleEarth/sliderule/releases/tag/v4.0.4)

## Benchmarks

```
atl06_aoi
	output:              571661 x 15 elements
	total:               37.352343 secs
	gdf2poly:            0.000495 secs
	toregion:            0.187156 secs
	__parse_native:      17.967412 secs
	todataframe:         0.683151 secs
	merge:               0.000002 secs
	flatten:             2.631864 secs
	atl06p:              37.137683 secs
atl06_ancillary
	output:              1180 x 16 elements
	total:               2.789055 secs
	gdf2poly:            0.000495 secs
	toregion:            0.187156 secs
	__parse_native:      0.584189 secs
	todataframe:         0.006167 secs
	merge:               0.018061 secs
	flatten:             0.030961 secs
	atl06p:              2.669654 secs
atl03_ancillary
	output:              1180 x 16 elements
	total:               3.039502 secs
	gdf2poly:            0.000495 secs
	toregion:            0.187156 secs
	__parse_native:      0.204709 secs
	todataframe:         0.006145 secs
	merge:               0.017784 secs
	flatten:             0.030781 secs
	atl06p:              3.038152 secs
atl06_parquet
	output:              1577 x 16 elements
	total:               2.704171 secs
	gdf2poly:            0.000495 secs
	toregion:            0.187156 secs
	__parse_native:      0.208408 secs
	todataframe:         0.006145 secs
	merge:               0.017784 secs
	flatten:             0.096728 secs
	atl06p:              2.703798 secs
atl03_parquet
	output:              22833 x 23 elements
	total:               1.926006 secs
	gdf2poly:            0.000495 secs
	toregion:            0.187156 secs
	__parse_native:      0.011052 secs
	todataframe:         0.006145 secs
	merge:               0.017784 secs
	flatten:             0.096728 secs
	atl06p:              2.703798 secs
	atl03sp:             1.908603 secs
atl06_sample_landsat
	output:              914 x 19 elements
	total:               12.250066 secs
	gdf2poly:            0.000495 secs
	toregion:            0.187156 secs
	__parse_native:      0.387278 secs
	todataframe:         0.006386 secs
	merge:               0.015527 secs
	flatten:             0.034550 secs
	atl06p:              4.577109 secs
	atl03sp:             1.908603 secs
atl06_sample_zonal_arcticdem
	output:              1651 x 26 elements
	total:               8.280844 secs
	gdf2poly:            0.000337 secs
	toregion:            0.005492 secs
	__parse_native:      0.309996 secs
	todataframe:         0.007301 secs
	merge:               0.025982 secs
	flatten:             0.050954 secs
	atl06p:              8.273125 secs
	atl03sp:             1.908603 secs
atl06_sample_nn_arcticdem
	output:              1651 x 19 elements
	total:               4.964582 secs
	gdf2poly:            0.000344 secs
	toregion:            0.005554 secs
	__parse_native:      0.085348 secs
	todataframe:         0.006952 secs
	merge:               0.023732 secs
	flatten:             0.043986 secs
	atl06p:              4.957544 secs
	atl03sp:             1.908603 secs
atl06_msample_nn_arcticdem
	output:              1526622 x 19 elements
	total:               151.426132 secs
	gdf2poly:            0.000327 secs
	toregion:            0.005378 secs
	__parse_native:      81.071236 secs
	todataframe:         2.486024 secs
	merge:               17.094814 secs
	flatten:             30.848135 secs
	atl06p:              150.431377 secs
	atl03sp:             1.908603 secs
atl06_no_sample_arcticdem
	output:              1526622 x 15 elements
	total:               62.244200 secs
	gdf2poly:            0.000369 secs
	toregion:            0.006280 secs
	__parse_native:      46.702178 secs
	todataframe:         1.159332 secs
	merge:               0.000003 secs
	flatten:             6.242216 secs
	atl06p:              61.391358 secs
	atl03sp:             1.908603 secs
atl03_rasterized_subset
	output:              51968 x 21 elements
	total:               16.987414 secs
	gdf2poly:            0.000369 secs
	toregion:            0.006280 secs
	__parse_native:      1.420302 secs
	todataframe:         0.029791 secs
	merge:               0.000003 secs
	flatten:             0.278818 secs
	atl06p:              61.391358 secs
	atl03sp:             16.692929 secs
atl03_polygon_subset
	output:              50799 x 21 elements
	total:               2.615179 secs
	gdf2poly:            0.000369 secs
	toregion:            0.006280 secs
	__parse_native:      1.153502 secs
	todataframe:         0.030567 secs
	merge:               0.000003 secs
	flatten:             0.276284 secs
	atl06p:              61.391358 secs
	atl03sp:             2.595944 secs
```