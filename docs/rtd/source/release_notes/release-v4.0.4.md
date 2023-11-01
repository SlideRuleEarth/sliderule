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
	total:               43.814599 secs
	gdf2poly:            0.000493 secs
	toregion:            0.179906 secs
	__parse_native:      17.863189 secs
	todataframe:         0.686153 secs
	merge:               0.000002 secs
	flatten:             2.573025 secs
	atl06p:              43.602137 secs
atl06_ancillary
	output:              1180 x 16 elements
	total:               3.390998 secs
	gdf2poly:            0.000493 secs
	toregion:            0.179906 secs
	__parse_native:      0.583039 secs
	todataframe:         0.006380 secs
	merge:               0.017860 secs
	flatten:             0.030841 secs
	atl06p:              3.266354 secs
atl03_ancillary
	output:              1180 x 16 elements
	total:               3.439100 secs
	gdf2poly:            0.000493 secs
	toregion:            0.179906 secs
	__parse_native:      0.203428 secs
	todataframe:         0.006245 secs
	merge:               0.017414 secs
	flatten:             0.030160 secs
	atl06p:              3.437936 secs
atl06_parquet
	output:              1577 x 16 elements
	total:               2.811472 secs
	gdf2poly:            0.000493 secs
	toregion:            0.179906 secs
	__parse_native:      0.207739 secs
	todataframe:         0.006245 secs
	merge:               0.017414 secs
	flatten:             0.098038 secs
	atl06p:              2.811012 secs
atl03_parquet
	output:              22833 x 23 elements
	total:               1.725062 secs
	gdf2poly:            0.000493 secs
	toregion:            0.179906 secs
	__parse_native:      0.010966 secs
	todataframe:         0.006245 secs
	merge:               0.017414 secs
	flatten:             0.098038 secs
	atl06p:              2.811012 secs
	atl03sp:             1.707698 secs
atl06_sample_landsat
	output:              914 x 19 elements
	total:               17.917378 secs
	gdf2poly:            0.000493 secs
	toregion:            0.179906 secs
	__parse_native:      0.388135 secs
	todataframe:         0.006484 secs
	merge:               0.014993 secs
	flatten:             0.033814 secs
	atl06p:              6.903303 secs
	atl03sp:             1.707698 secs
atl06_sample_zonal_arcticdem
	output:              1651 x 26 elements
	total:               10.080642 secs
	gdf2poly:            0.000349 secs
	toregion:            0.005339 secs
	__parse_native:      0.311334 secs
	todataframe:         0.007412 secs
	merge:               0.025494 secs
	flatten:             0.050405 secs
	atl06p:              10.073074 secs
	atl03sp:             1.707698 secs
atl06_sample_nn_arcticdem
	output:              1651 x 19 elements
	total:               5.264215 secs
	gdf2poly:            0.000330 secs
	toregion:            0.005426 secs
	__parse_native:      0.085615 secs
	todataframe:         0.007111 secs
	merge:               0.023432 secs
	flatten:             0.043231 secs
	atl06p:              5.257288 secs
	atl03sp:             1.707698 secs
atl06_msample_nn_arcticdem
	output:              1526622 x 19 elements
	total:               196.288210 secs
	gdf2poly:            0.000333 secs
	toregion:            0.005315 secs
	__parse_native:      81.371897 secs
	todataframe:         2.368329 secs
	merge:               16.774900 secs
	flatten:             29.863883 secs
	atl06p:              195.266846 secs
	atl03sp:             1.707698 secs
atl06_no_sample_arcticdem
	output:              1526622 x 15 elements
	total:               67.067643 secs
	gdf2poly:            0.000357 secs
	toregion:            0.006271 secs
	__parse_native:      46.557475 secs
	todataframe:         1.148960 secs
	merge:               0.000003 secs
	flatten:             6.094961 secs
	atl06p:              66.195801 secs
	atl03sp:             1.707698 secs
atl03_rasterized_subset
	output:              51968 x 21 elements
	total:               5.100526 secs
	gdf2poly:            0.000357 secs
	toregion:            0.006271 secs
	__parse_native:      1.429852 secs
	todataframe:         0.029469 secs
	merge:               0.000003 secs
	flatten:             0.273626 secs
	atl06p:              66.195801 secs
	atl03sp:             4.785053 secs
atl03_polygon_subset
	output:              50799 x 21 elements
	total:               2.951564 secs
	gdf2poly:            0.000357 secs
	toregion:            0.006271 secs
	__parse_native:      1.164287 secs
	todataframe:         0.029286 secs
	merge:               0.000003 secs
	flatten:             0.272694 secs
	atl06p:              66.195801 secs
	atl03sp:             2.935580 secs
```