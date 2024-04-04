# Release v4.3.1

2024-03-08

Version description of the v4.3.1 release of ICESat-2 SlideRule.

## New Features

* [eb1085d](https://github.com/ICESat2-SlideRule/sliderule/commit/eb1085da18a3cd63939f340f811ccadae1c22715) - GEDI APIs for L1B and L2A datasets now point to LP DAAC S3 bucket.

* GeoParquet output now supports ancillary fields

* Added support for staging GeoParquet files in public S3 bucket via setting the output asset to "sliderule-public".

* ICESat-2 APIs support filtering data by beam (e.g. gt1l, gt1r, gt2l, gt2r, gt3l, gt3r)

* The Python client `sliderule.toregion` function now supports shapely Polygons

* Added `region` to output of `atl06p` and `atl03sp` APIs; makes it easier to determine direction of travel for each measurement in addition to easily reconstructing which granule a measurement comes from

* [878a48b](https://github.com/ICESat2-SlideRule/sliderule/commit/878a48b4474e6a6a7b598e4abdddcceb8b438697) - Python client can now be configured to rethrow exceptions for applications that want to manage exceptions themselves

* SlideRule web APIs now support CORS as well as JavaScript clients that can only provide data payloads on POST requests

## Issues Resolved

* [24a6c79](https://github.com/ICESat2-SlideRule/sliderule/commit/24a6c798059a13bc81cd9feae09e96ab5a78461a) - fixed hanging issue when terminator is dropped due to queue being full

* [25640d1](https://github.com/ICESat2-SlideRule/sliderule/commit/25640d123e4b9f015edad683f141f3ecba45ec69) - fixed corner-case bug in ATL08 processing when no ancillary data is available.

* [599f4d7](https://github.com/ICESat2-SlideRule/sliderule/commit/599f4d778d726a12887b2a63af5fc59aec288026) - fixed memory leak with atl08 ancillary fields

* [407da7d](https://github.com/ICESat2-SlideRule/sliderule/commit/407da7d6bedf27d898bc0f5939321af6686b3005) - fixed bug in ABoVE classifier code that was causing out of bounds memory access

* [e94161a](https://github.com/ICESat2-SlideRule/sliderule/commit/e94161ae0506df39e66001341fe293e8dde1270a) - fixed bug in GeoParquet geometry code where only the first latitude,longitude of each batch was being used for the batch; it caused the data to stack up on top of each other and look decimated

* [368](https://github.com/ICESat2-SlideRule/sliderule/issues/368) - fixed bug in PhoREAL implementation ancillary field interpolation

## Development Updates

* [e9ec583](https://github.com/ICESat2-SlideRule/sliderule/commit/e9ec583e0b136f66d429281bf6492dd12f1880dc) - reordered build and deploy action so that temporary token is still valid when uploading terraform

* [381](https://github.com/ICESat2-SlideRule/sliderule/pull/381) - static website move from ECS to CloudFront

* GeoParquet output no longer includes latitude and longitude columns that duplicate what is provided in the geometry column

* [e055bc6](https://github.com/ICESat2-SlideRule/sliderule/commit/e055bc6e419f13c0f95ab0ba60ada8897bfa2aa9) - meta data for each record (e.g. coordinates, index, etc) is now indicated in the definition of the record and no longer provided in each endpoint lua file

* [00ecdb3](https://github.com/ICESat2-SlideRule/sliderule/commit/00ecdb34d3cefca4ca35b232c149e32abac05567) - server.lua script waits to authenticate to DAACs until the IAM role credentials have been established; this prevents errors at startup when trying to fetch the netrc file

* [08b56f7](https://github.com/ICESat2-SlideRule/sliderule/commit/08b56f70880d9b86ace32f318805357a5d603193) - fixed disk usage metric display in Grafana

* [0b00277](https://github.com/ICESat2-SlideRule/sliderule/commit/0b00277df7c040cc4fe06abf7eadd4a665d0d76f) - fixed prometheus endpoint to return valid table

* [373](https://github.com/ICESat2-SlideRule/sliderule/issues/373) - removed `maxi` from all examples

## Getting This Release

[https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v4.3.1](https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v4.3.1)

## Benchmarks

```
atl06_aoi
        output:              625445 x 16 elements
        total:               26.377888 secs
        gdf2poly:            0.000519 secs
        toregion:            0.163608 secs
        __parse_native:      21.453290 secs
        todataframe:         0.406155 secs
        merge:               0.000003 secs
        flatten:             2.858293 secs
        atl06p:              26.116402 secs
atl06_ancillary
        output:              1180 x 17 elements
        total:               2.853569 secs
        gdf2poly:            0.000519 secs
        toregion:            0.163608 secs
        __parse_native:      0.733802 secs
        todataframe:         0.006865 secs
        merge:               0.016739 secs
        flatten:             0.035448 secs
        atl06p:              2.649313 secs
atl03_ancillary
        output:              1180 x 17 elements
        total:               2.746311 secs
        gdf2poly:            0.000519 secs
        toregion:            0.163608 secs
        __parse_native:      0.159768 secs
        todataframe:         0.007559 secs
        merge:               0.017402 secs
        flatten:             0.037174 secs
        atl06p:              2.744864 secs
atl06_parquet
        output:              1577 x 17 elements
        total:               2.649226 secs
        gdf2poly:            0.000519 secs
        toregion:            0.163608 secs
        __parse_native:      0.210186 secs
        todataframe:         0.007559 secs
        merge:               0.017402 secs
        flatten:             0.041889 secs
        atl06p:              2.648749 secs
atl03_parquet
        output:              22833 x 22 elements
        total:               1.534764 secs
        gdf2poly:            0.000519 secs
        toregion:            0.163608 secs
        __parse_native:      0.011159 secs
        todataframe:         0.007559 secs
        merge:               0.017402 secs
        flatten:             0.041889 secs
        atl06p:              2.648749 secs
        atl03sp:             1.507323 secs
atl06_sample_landsat
        output:              914 x 20 elements
        total:               6.381781 secs
        gdf2poly:            0.000519 secs
        toregion:            0.163608 secs
        __parse_native:      0.373254 secs
        todataframe:         0.008152 secs
        merge:               0.015304 secs
        flatten:             0.037495 secs
        atl06p:              4.657887 secs
        atl03sp:             1.507323 secs
atl06_sample_zonal_arcticdem
        output:              1651 x 27 elements
        total:               5.275135 secs
        gdf2poly:            0.000451 secs
        toregion:            0.006655 secs
        __parse_native:      0.326858 secs
        todataframe:         0.007310 secs
        merge:               0.023985 secs
        flatten:             0.050125 secs
        atl06p:              5.266107 secs
        atl03sp:             1.507323 secs
atl06_sample_nn_arcticdem
        output:              1651 x 20 elements
        total:               4.773236 secs
        gdf2poly:            0.000421 secs
        toregion:            0.006201 secs
        __parse_native:      0.092055 secs
        todataframe:         0.008839 secs
        merge:               0.026527 secs
        flatten:             0.051369 secs
        atl06p:              4.764687 secs
        atl03sp:             1.507323 secs
atl06_msample_nn_arcticdem
        output:              1596050 x 20 elements
        total:               125.249892 secs
        gdf2poly:            0.000456 secs
        toregion:            0.007406 secs
        __parse_native:      89.351120 secs
        todataframe:         1.128149 secs
        merge:               15.596906 secs
        flatten:             29.527919 secs
        atl06p:              124.077317 secs
        atl03sp:             1.507323 secs
atl06_no_sample_arcticdem
        output:              1596050 x 16 elements
        total:               64.487345 secs
        gdf2poly:            0.000436 secs
        toregion:            0.007185 secs
        __parse_native:      53.242468 secs
        todataframe:         0.967663 secs
        merge:               0.000003 secs
        flatten:             7.009740 secs
        atl06p:              63.322487 secs
        atl03sp:             1.507323 secs
atl03_rasterized_subset
        output:              51968 x 22 elements
        total:               3.796079 secs
        gdf2poly:            0.000436 secs
        toregion:            0.007185 secs
        __parse_native:      1.657236 secs
        todataframe:         0.038700 secs
        merge:               0.000003 secs
        flatten:             0.321405 secs
        atl06p:              63.322487 secs
        atl03sp:             3.282703 secs
atl03_polygon_subset
        output:              50799 x 22 elements
        total:               3.085772 secs
        gdf2poly:            0.000436 secs
        toregion:            0.007185 secs
        __parse_native:      1.390404 secs
        todataframe:         0.041605 secs
        merge:               0.000003 secs
        flatten:             0.349935 secs
        atl06p:              63.322487 secs
        atl03sp:             3.056956 secs
```