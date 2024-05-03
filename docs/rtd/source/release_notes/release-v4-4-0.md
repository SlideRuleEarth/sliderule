# Release v4.4.0

2024-04-04

Version description of the v4.4.0 release of SlideRule Earth.

## New Features

* Resources are queried from servers instead of client. If a processing request does not include a list of `resources` to process, the server processing the request will query CMR and populate the `resources` parameter.  In addition, any sampling requests that need a populated `catalog` parameter will also be queried on the server side and have that parameter populated.

* [389](https://github.com/SlideRuleEarth/sliderule/pull/389) and [383](https://github.com/SlideRuleEarth/sliderule/pull/383) - updates to demo plotting and added support for downloading results

* Raster sampling support when the output is an Arrow generated format (Geo/Parquet, CSV, Feather).

* Added Feather output support

* [43d536b](https://github.com/SlideRuleEarth/sliderule/commit/43d536b690ac4fe8337634980092bbf164ece366) - Request parameters and record information added to metadata of generated Parquet files.

* [763e553](https://github.com/SlideRuleEarth/sliderule/commit/763e5537d23483601535f622d9386f93456ba967) - max confidence in the signal_conf variable can be selected when filtering ATL03 photons based on confidence level

* [392](https://github.com/SlideRuleEarth/sliderule/pull/392) - GEBCO raster sampling support added

* [9d71b6e](https://github.com/SlideRuleEarth/sliderule/commit/9d71b6ef8da004c18a2a40f18035ad3eb3d36b66) - Meta global canopy height raster support added

## Issues Resolved

* Logs returned to users are now consistently implemented as exception records through the `alert` function.

## Development Updates

* GDAL headers removed from exported package headers

* GDAL log messages are defaulted off

## Getting This Release

[https://github.com/SlideRuleEarth/sliderule/releases/tag/v4.4.0](https://github.com/SlideRuleEarth/sliderule/releases/tag/v4.4.0)

## Benchmarks
```
atl06_aoi
	output:              657468 x 16 elements
	total:               30.217794 secs
	gdf2poly:            0.000361 secs
	toregion:            0.101621 secs
	__parse_native:      22.553648 secs
	todataframe:         0.406877 secs
	merge:               0.000003 secs
	flatten:             2.973291 secs
	atl06p:              29.944871 secs
atl06_ancillary
	output:              1180 x 17 elements
	total:               3.515006 secs
	gdf2poly:            0.000361 secs
	toregion:            0.101621 secs
	__parse_native:      0.706065 secs
	todataframe:         0.005833 secs
	merge:               0.014554 secs
	flatten:             0.032062 secs
	atl06p:              3.311915 secs
atl03_ancillary
	output:              1180 x 17 elements
	total:               2.783418 secs
	gdf2poly:            0.000361 secs
	toregion:            0.101621 secs
	__parse_native:      0.164518 secs
	todataframe:         0.005624 secs
	merge:               0.013533 secs
	flatten:             0.030675 secs
	atl06p:              2.782245 secs
atl06_parquet
	output:              1577 x 17 elements
	total:               2.658988 secs
	gdf2poly:            0.000361 secs
	toregion:            0.101621 secs
	__parse_native:      0.209843 secs
	todataframe:         0.005624 secs
	merge:               0.013533 secs
	flatten:             0.043108 secs
	atl06p:              2.658509 secs
atl03_parquet
	output:              22833 x 22 elements
	total:               1.645541 secs
	gdf2poly:            0.000361 secs
	toregion:            0.101621 secs
	__parse_native:      0.010994 secs
	todataframe:         0.005624 secs
	merge:               0.013533 secs
	flatten:             0.043108 secs
	atl06p:              2.658509 secs
	atl03sp:             1.618785 secs
atl06_sample_landsat
	output:              914 x 20 elements
	total:               6.421875 secs
	gdf2poly:            0.000361 secs
	toregion:            0.101621 secs
	__parse_native:      0.371876 secs
	todataframe:         0.005620 secs
	merge:               0.011310 secs
	flatten:             0.030409 secs
	atl06p:              4.646134 secs
	atl03sp:             1.618785 secs
atl06_sample_zonal_arcticdem
	output:              1651 x 27 elements
	total:               7.677317 secs
	gdf2poly:            0.000307 secs
	toregion:            0.005774 secs
	__parse_native:      0.325183 secs
	todataframe:         0.006104 secs
	merge:               0.020617 secs
	flatten:             0.046536 secs
	atl06p:              7.669182 secs
	atl03sp:             1.618785 secs
atl06_sample_nn_arcticdem
	output:              1651 x 20 elements
	total:               5.417307 secs
	gdf2poly:            0.000285 secs
	toregion:            0.005677 secs
	__parse_native:      0.094793 secs
	todataframe:         0.006071 secs
	merge:               0.019175 secs
	flatten:             0.039384 secs
	atl06p:              5.409957 secs
	atl03sp:             1.618785 secs
atl06_msample_nn_arcticdem
	output:              1666932 x 20 elements
	total:               134.748328 secs
	gdf2poly:            0.000317 secs
	toregion:            0.005744 secs
	__parse_native:      96.033735 secs
	todataframe:         1.105081 secs
	merge:               15.617355 secs
	flatten:             30.201550 secs
	atl06p:              133.563389 secs
	atl03sp:             1.618785 secs
atl06_no_sample_arcticdem
	output:              1666932 x 16 elements
	total:               71.098669 secs
	gdf2poly:            0.000309 secs
	toregion:            0.006578 secs
	__parse_native:      57.098009 secs
	todataframe:         0.983960 secs
	merge:               0.000003 secs
	flatten:             7.547646 secs
	atl06p:              69.954445 secs
	atl03sp:             1.618785 secs
atl03_rasterized_subset
	output:              51968 x 22 elements
	total:               3.760473 secs
	gdf2poly:            0.000309 secs
	toregion:            0.006578 secs
	__parse_native:      1.690030 secs
	todataframe:         0.035699 secs
	merge:               0.000003 secs
	flatten:             0.324365 secs
	atl06p:              69.954445 secs
	atl03sp:             3.284419 secs
atl03_polygon_subset
	output:              50799 x 22 elements
	total:               2.734295 secs
	gdf2poly:            0.000309 secs
	toregion:            0.006578 secs
	__parse_native:      1.232244 secs
	todataframe:         0.035859 secs
	merge:               0.000003 secs
	flatten:             0.320961 secs
	atl06p:              69.954445 secs
	atl03sp:             2.711521 secs
```