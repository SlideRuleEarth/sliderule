# Release v4.20.0

2025-11-01

Version description of the v4.20.0 release of SlideRule Earth.

## Major Changes

* SlideRule now defaults to using release `007` for applicable ICESat-2 Standard Data Products.

## New Functionality

* [#541](https://github.com/SlideRuleEarth/sliderule/pull/541) - LAS/LAZ Support

* [#528](https://github.com/SlideRuleEarth/sliderule/pull/528) - CRS correctness in GeoParquets and GeoDataFrames

* [c4581dd](https://github.com/SlideRuleEarth/sliderule/commit/c4581dd876a63addad632b36472e2c196eaa166c) - Created `execre` endpoint to execute custom containers

## Issues Resolved

* [7d8c96c](https://github.com/SlideRuleEarth/sliderule/commit/7d8c96c7f47068aa22017bb3a12c1d24d4677ce8) - Updated playwright version to address vulnerability

* Added ATL24 support to the Python client earthdata module

* [faf1de0](https://github.com/SlideRuleEarth/sliderule/commit/faf1de0194719f5bcf7dcea33131b7c89b4d7540) - Fixed errant CMR failure status message

* [8856215](https://github.com/SlideRuleEarth/sliderule/commit/8856215f2aa9a67ba90bfddba23a74d614a9a132) - fix for with_flags and bands in dataframe sampling

## Development Changes

* [#543](https://github.com/SlideRuleEarth/sliderule/pull/543) - Migrated use of terraform to cloud formation

* [#521](https://github.com/SlideRuleEarth/sliderule/pull/521) - GitHub actions for Manager and AMS

* GitHub action for nodejs test

* Moved AMS, Certbot, ILB, Manager applications into their own applications directory

* Lazy loaded CRS files

* [7f740af](https://github.com/SlideRuleEarth/sliderule/commit/7f740af7b8ea049e477ddbbe0cba9bed1dea439b) - Returning http error codes from non-streaming endpoints

* [da04ca3](https://github.com/SlideRuleEarth/sliderule/commit/da04ca34b8a25b1072d4ebaaf55ea169a5668b9a) - Automatic loading of plugins from well known directory in S3

* [90cf537](https://github.com/SlideRuleEarth/sliderule/commit/90cf5377c19f1395887d315ebfec4c2edfcf2609) - Autoheal restarts unhealthy containers

## Getting This Release

[https://github.com/SlideRuleEarth/sliderule/releases/tag/v4.20.0](https://github.com/SlideRuleEarth/sliderule/releases/tag/v4.20.0)

## Benchmarks
> clients/python/utils/benchmark.py
```
atl06_aoi <766208 x 16> - 29.720592 secs
atl06_ancillary <916 x 17> - 2.843990 secs
atl03_ancillary <916 x 17> - 2.525772 secs
atl06_parquet <1600 x 18> - 2.759987 secs
atl03_parquet <23072 x 23> - 1.525421 secs
atl06_sample_landsat <916 x 20> - 6.121840 secs
atl06_sample_zonal_arcticdem <1695 x 27> - 4.653745 secs
atl06_sample_nn_arcticdem <1695 x 20> - 4.491588 secs
atl06_msample_nn_arcticdem <1695 x 20> - 4.551552 secs
atl06_no_sample_arcticdem <1695 x 16> - 2.524664 secs
atl03_rasterized_subset <51832 x 22> - 2.345279 secs
atl03_polygon_subset <50615 x 22> - 2.094441 secs
```

## Baseline
> clients/python/utils/baseline.py
```
GEDI / 3DEP = 2660.2507194650643
ICESat-2 / ArcticDEM = 1598.2301327720206
ICESat-2 / ATL06p = 1809.893690173468
ICESat-2 / PhoREAL = 3.245903730392456
```