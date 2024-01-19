# Release v4.2.0

2024-01-18

Version description of the v4.2.0 release of ICESat-2 SlideRule.

## Issues Resolved

* [156](https://github.com/ICESat2-SlideRule/sliderule/issues/156), [117](https://github.com/ICESat2-SlideRule/sliderule/issues/117) - large processing runs on full granules now lock the entire node they are running on

* [df1f83e](https://github.com/ICESat2-SlideRule/sliderule/commit/df1f83e6c8572f9deffb5d48ac3bde5f41a7c1fc) - logging in Python client behaves more consistently and is more intuitive

* [0f8039d](https://github.com/ICESat2-SlideRule/sliderule/commit/0f8039dfdc3c97f5a8a6183660fdc9a89925a1ae) - the server code now uses Instance Metadata Service Version 2 (IMDSv2) for accessing metadata associated with the EC2 it is running on.  This is required for some instance types and allows SlideRule to be run on GPU enhanced instances.

## Development Updates

* Node.js client now supports a basic ATL06 API call.

* Amazon Linux 2023 is the new baseline image for the EC2 instances as well as the SlideRule docker container.  This provides up-to-date tooling for building the code, and the latest packages for running in the AWS environment.

* [287](https://github.com/ICESat2-SlideRule/sliderule/issues/287) - dependencies in the build environment are captured in a lock file

## Getting This Release

[https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v4.2.0](https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v4.2.0)

## Benchmarks

```
atl06_aoi
	output:              593360 x 15 elements
	total:               23.183898 secs
	gdf2poly:            0.000459 secs
	toregion:            0.154400 secs
	__parse_native:      18.721783 secs
	todataframe:         0.378082 secs
	merge:               0.000003 secs
	flatten:             2.620263 secs
	atl06p:              22.942842 secs
atl06_ancillary
	output:              1180 x 16 elements
	total:               2.926518 secs
	gdf2poly:            0.000459 secs
	toregion:            0.154400 secs
	__parse_native:      0.693103 secs
	todataframe:         0.006511 secs
	merge:               0.016543 secs
	flatten:             0.034089 secs
	atl06p:              2.747429 secs
atl03_ancillary
	output:              1180 x 16 elements
	total:               2.741479 secs
	gdf2poly:            0.000459 secs
	toregion:            0.154400 secs
	__parse_native:      0.155448 secs
	todataframe:         0.006423 secs
	merge:               0.015983 secs
	flatten:             0.033555 secs
	atl06p:              2.740380 secs
atl06_parquet
	output:              1577 x 16 elements
	total:               2.547042 secs
	gdf2poly:            0.000459 secs
	toregion:            0.154400 secs
	__parse_native:      0.209621 secs
	todataframe:         0.006423 secs
	merge:               0.015983 secs
	flatten:             0.039555 secs
	atl06p:              2.546637 secs
atl03_parquet
	output:              22833 x 23 elements
	total:               1.528019 secs
	gdf2poly:            0.000459 secs
	toregion:            0.154400 secs
	__parse_native:      0.011802 secs
	todataframe:         0.006423 secs
	merge:               0.015983 secs
	flatten:             0.039555 secs
	atl06p:              2.546637 secs
	atl03sp:             1.507086 secs
atl06_sample_landsat
	output:              914 x 19 elements
	total:               7.718503 secs
	gdf2poly:            0.000459 secs
	toregion:            0.154400 secs
	__parse_native:      0.366363 secs
	todataframe:         0.006160 secs
	merge:               0.013839 secs
	flatten:             0.032872 secs
	atl06p:              4.651022 secs
	atl03sp:             1.507086 secs
atl06_sample_zonal_arcticdem
	output:              1651 x 26 elements
	total:               5.278018 secs
	gdf2poly:            0.000381 secs
	toregion:            0.005700 secs
	__parse_native:      0.313986 secs
	todataframe:         0.007171 secs
	merge:               0.023651 secs
	flatten:             0.049567 secs
	atl06p:              5.269944 secs
	atl03sp:             1.507086 secs
atl06_sample_nn_arcticdem
	output:              1651 x 19 elements
	total:               4.862944 secs
	gdf2poly:            0.000354 secs
	toregion:            0.005774 secs
	__parse_native:      0.086582 secs
	todataframe:         0.006879 secs
	merge:               0.021548 secs
	flatten:             0.041994 secs
	atl06p:              4.855308 secs
	atl03sp:             1.507086 secs
atl06_msample_nn_arcticdem
	output:              1540621 x 19 elements
	total:               116.179606 secs
	gdf2poly:            0.000339 secs
	toregion:            0.005342 secs
	__parse_native:      82.336831 secs
	todataframe:         1.096330 secs
	merge:               14.994802 secs
	flatten:             27.845451 secs
	atl06p:              115.044704 secs
	atl03sp:             1.507086 secs
atl06_no_sample_arcticdem
	output:              1540621 x 15 elements
	total:               59.035230 secs
	gdf2poly:            0.000412 secs
	toregion:            0.006899 secs
	__parse_native:      48.609775 secs
	todataframe:         0.973664 secs
	merge:               0.000002 secs
	flatten:             6.840683 secs
	atl06p:              57.919001 secs
	atl03sp:             1.507086 secs
atl03_rasterized_subset
	output:              51968 x 21 elements
	total:               3.678880 secs
	gdf2poly:            0.000412 secs
	toregion:            0.006899 secs
	__parse_native:      1.429582 secs
	todataframe:         0.039154 secs
	merge:               0.000002 secs
	flatten:             0.313029 secs
	atl06p:              57.919001 secs
	atl03sp:             3.224579 secs
atl03_polygon_subset
	output:              50799 x 21 elements
	total:               2.734070 secs
	gdf2poly:            0.000412 secs
	toregion:            0.006899 secs
	__parse_native:      1.178744 secs
	todataframe:         0.039848 secs
	merge:               0.000002 secs
	flatten:             0.317400 secs
	atl06p:              57.919001 secs
	atl03sp:             2.711248 secs
```