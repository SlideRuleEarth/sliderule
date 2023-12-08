# Release v4.1.0

2023-12-07

Version description of the v4.1.0 release of ICESat-2 SlideRule.

***Important**: This version requires an update of the Python client to use.  The underlying mechanism used in support of including ancillary fields in processing requests was updated to support both the PhoREAL algorithm and the ATL06 subsetter.  As a result, in order to include ancillary field requests in your code, you must have the latest client installed.  No changes are needed to the code in your scripts.

## New Functionality

* [#356](https://github.com/ICESat2-SlideRule/sliderule/pull/356) - Updated functionality in `ipysliderule` module to support AGU CryoCloud Workshop.

* ATL06 Subsetting API - `atl06s` and `atl06sp` provide direct subsetting of ATL06 standard data product

* ATL08 / PhoREAL Ancillary Fields - the `atl08` and `atl08p` APIs support ancillary fields via the `atl08_fields` parameter

* [b9fec20](https://github.com/ICESat2-SlideRule/sliderule/commit/b9fec20b464a0d89ac5ad1cee2d31f96788cb01a) [81ffae6](https://github.com/ICESat2-SlideRule/sliderule/commit/81ffae68cf1d9c6783502b51b585723c0c8630bf) - Added functionality to support large polygons when subsetting; this is useful when the list of granules to process is produced through some other means than a geospatial query, but the returned granules need to be subsetted to a large region like everything below/above a certain latitude.

## Issues Resolved

* [98c7922](https://github.com/ICESat2-SlideRule/sliderule/commit/98c792264eba79949c8c79d66b7c7642250c51b2) - fixed plugin version check

* [#357]https://github.com/ICESat2-SlideRule/sliderule/issues/357) - fixed pflags in ATL06-SR

## Development Updates

* Pixel level access and subsetting added to raster functionality

* [#156](https://github.com/ICESat2-SlideRule/sliderule/issues/156) - implemented full granule locking

* [#287](https://github.com/ICESat2-SlideRule/sliderule/issues/287) - capture build dependency versions in a lock file

* [e5dce17](https://github.com/ICESat2-SlideRule/sliderule/commit/e5dce17c9d797cb4696e9a41dbf26915e6704e74) - standardized on C++17 for all plugins

* Unit tests are only run in the self test when compiled as a Debug build 

* Consolidated build system so that only a single makefile is used to build the sliderule server for the AWS target

## Getting This Release

[https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v4.1.0](https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v4.1.0)

## Benchmarks

```
atl06_aoi
	output:              593360 x 15 elements
	total:               30.233044 secs
	gdf2poly:            0.000564 secs
	toregion:            0.094093 secs
	__parse_native:      18.946666 secs
	todataframe:         0.381974 secs
	merge:               0.000003 secs
	flatten:             2.622699 secs
	atl06p:              29.950796 secs
atl06_ancillary
	output:              1180 x 16 elements
	total:               3.034162 secs
	gdf2poly:            0.000564 secs
	toregion:            0.094093 secs
	__parse_native:      0.694203 secs
	todataframe:         0.006470 secs
	merge:               0.016675 secs
	flatten:             0.034338 secs
	atl06p:              2.841465 secs
atl03_ancillary
	output:              1180 x 16 elements
	total:               3.244222 secs
	gdf2poly:            0.000564 secs
	toregion:            0.094093 secs
	__parse_native:      0.157462 secs
	todataframe:         0.006687 secs
	merge:               0.016526 secs
	flatten:             0.034690 secs
	atl06p:              3.242928 secs
atl06_parquet
	output:              1577 x 16 elements
	total:               2.648922 secs
	gdf2poly:            0.000564 secs
	toregion:            0.094093 secs
	__parse_native:      0.209142 secs
	todataframe:         0.006687 secs
	merge:               0.016526 secs
	flatten:             0.038423 secs
	atl06p:              2.648496 secs
atl03_parquet
	output:              22833 x 23 elements
	total:               1.728604 secs
	gdf2poly:            0.000564 secs
	toregion:            0.094093 secs
	__parse_native:      0.012078 secs
	todataframe:         0.006687 secs
	merge:               0.016526 secs
	flatten:             0.038423 secs
	atl06p:              2.648496 secs
	atl03sp:             1.707236 secs
atl06_sample_landsat
	output:              914 x 19 elements
	total:               6.773184 secs
	gdf2poly:            0.000564 secs
	toregion:            0.094093 secs
	__parse_native:      0.367053 secs
	todataframe:         0.006017 secs
	merge:               0.013917 secs
	flatten:             0.032864 secs
	atl06p:              5.650639 secs
	atl03sp:             1.707236 secs
atl06_sample_zonal_arcticdem
	output:              1651 x 26 elements
	total:               5.172106 secs
	gdf2poly:            0.000421 secs
	toregion:            0.005902 secs
	__parse_native:      0.314454 secs
	todataframe:         0.006817 secs
	merge:               0.023643 secs
	flatten:             0.048558 secs
	atl06p:              5.163591 secs
	atl03sp:             1.707236 secs
atl06_sample_nn_arcticdem
	output:              1651 x 19 elements
	total:               4.964410 secs
	gdf2poly:            0.000380 secs
	toregion:            0.005617 secs
	__parse_native:      0.088379 secs
	todataframe:         0.006685 secs
	merge:               0.021653 secs
	flatten:             0.042214 secs
	atl06p:              4.956937 secs
	atl03sp:             1.707236 secs
atl06_msample_nn_arcticdem
	output:              1540621 x 19 elements
	total:               149.481370 secs
	gdf2poly:            0.000345 secs
	toregion:            0.005443 secs
	__parse_native:      82.972866 secs
	todataframe:         1.041812 secs
	merge:               14.907858 secs
	flatten:             27.508495 secs
	atl06p:              148.264367 secs
	atl03sp:             1.707236 secs
atl06_no_sample_arcticdem
	output:              1540621 x 15 elements
	total:               61.295447 secs
	gdf2poly:            0.000370 secs
	toregion:            0.006505 secs
	__parse_native:      48.411216 secs
	todataframe:         0.847016 secs
	merge:               0.000003 secs
	flatten:             6.459972 secs
	atl06p:              60.109332 secs
	atl03sp:             1.707236 secs
atl03_rasterized_subset
	output:              51968 x 21 elements
	total:               4.837523 secs
	gdf2poly:            0.000370 secs
	toregion:            0.006505 secs
	__parse_native:      1.639814 secs
	todataframe:         0.034828 secs
	merge:               0.000003 secs
	flatten:             0.317074 secs
	atl06p:              60.109332 secs
	atl03sp:             4.344942 secs
atl03_polygon_subset
	output:              50799 x 21 elements
	total:               2.869779 secs
	gdf2poly:            0.000370 secs
	toregion:            0.006505 secs
	__parse_native:      1.183934 secs
	todataframe:         0.033643 secs
	merge:               0.000003 secs
	flatten:             0.305770 secs
	atl06p:              60.109332 secs
	atl03sp:             2.847249 secs
```