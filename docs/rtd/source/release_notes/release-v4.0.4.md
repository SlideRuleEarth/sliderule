# Release v4.0.4

2023-09-26

Version description of the v4.0.4 release of ICESat-2 SlideRule.  This document also captures functionality added in versions v4.0.1, v4.0.2, and v4.0.3.

## Issues Resolved

* [f173932](https://github.com/ICESat2-SlideRule/sliderule/commit/f173932f98d85cb888fc8c921f69b15512acdfb5) - updates to support latest ipywidget release

* [dd0fdcd](https://github.com/ICESat2-SlideRule/sliderule/commit/dd0fdcdca69740068798c7c420dcd07906f71528) - debug message logged when request to ILB fails

* [3b535c5](https://github.com/ICESat2-SlideRule/sliderule/commit/3b535c588f9fa3c54e0f4ab86ea223c5f8463c23) - fix ATL06-SR bug when processing longitudes outside (-150, 150)

* [#342](https://github.com/ICESat2-SlideRule/sliderule/issues/342) - fixed broken links in documentation

* [1c41114](https://github.com/ICESat2-SlideRule/sliderule/commit/1c411140d211ba33658d8503a63f24536d198391) - default all raster datasets to us-west-2, unless overridden; saves a few cURL calls undernearth the hood

* [#345](https://github.com/ICESat2-SlideRule/sliderule/pull/345) - updated soft limits for number of open files to support large raster sampling requests

* [d2d9b63](https://github.com/ICESat2-SlideRule/sliderule/commit/d2d9b637cdeec2b041e7736e4e6d9af9794fa776) - GDAL multi-threaded support added for VRTs

* [264433b](https://github.com/ICESat2-SlideRule/sliderule/commit/264433b980998c5db6fe9ee2fc672ba436d8da52) - fixed bug in ATL06-SR where invalid memory was freed when a segment containing no photons was processed

* [5bbfaa6](https://github.com/ICESat2-SlideRule/sliderule/commit/5bbfaa6cf6aeee6b071c529b993afb458f6c5af6) - security vulnerability found by GitHub in node.js client fixed

* [d1e776b](https://github.com/ICESat2-SlideRule/sliderule/commit/d1e776b5fa0922c196b8230369d6b97af1cfd85f) - GeoJSONRaster optimized to faster subsetting of ATL03 via burned geojson

* [61fd6db](https://github.com/ICESat2-SlideRule/sliderule/commit/61fd6dbc5fcd3299f19790d72177f67a1b03b85f) - social login to provisioning system from the Python client is now supported

* CurlLib replaced HttpClient in Orchestrator and EndpointProxy, significantly improving performance

* [35671ed](https://github.com/ICESat2-SlideRule/sliderule/commit/35671ed32349fc8360bd8b4f8590b11dfc8974e5) - updated GEDI L4B data product directory entry to match latest release

## Development Updates

* [#338](https://github.com/ICESat2-SlideRule/sliderule/pull/338), [#339](https://github.com/ICESat2-SlideRule/sliderule/pull/339) - Codespell pre-commit hook added to sliderule repository; along with it a slew of spelling corrections.

* [#350](https://github.com/ICESat2-SlideRule/sliderule/pull/350) - major update to code base to support running clang-tidy, cppcheck, and address sanitizer on all Debug builds and selftests. 

* [75c7f7d](https://github.com/ICESat2-SlideRule/sliderule/commit/75c7f7d5e0dfb324d3fa5cd51a53e2a375c1a76f) - Initialization function is now a fixture in the pytests

* [#208](https://github.com/ICESat2-SlideRule/sliderule/issues/208) - Certificate for slideruleearth.io automatically rotated using certbot and AWS lambda

* [#251](https://github.com/ICESat2-SlideRule/sliderule/issues/251) - version information added to documentation

* [29de90f](https://github.com/ICESat2-SlideRule/sliderule/commit/29de90fc2d44561519fed63cbf2ecc8dadb11bed) - metric collection moved to orchestrator

## Getting This Release

[https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v4.0.4](https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v4.0.4)

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