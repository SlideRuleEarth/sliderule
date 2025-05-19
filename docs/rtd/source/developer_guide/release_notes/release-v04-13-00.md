# Release v4.13.0

2025-05-09

Version description of the v4.13.0 release of SlideRule Earth.

## New/Improved Functionality

* Usage statistics and trending now supported through a `manager` application which collects telemetry and stores it in a persistent DuckDB database that is maintained in our S3 bucket.

* [0478900](https://github.com/SlideRuleEarth/sliderule/commit/0478900801c3c09bd0bc8009e47a54c86e1d7b35) - GEDTM uses locally hosted rasters for significant performance increase

* [5d204f1](https://github.com/SlideRuleEarth/sliderule/commit/5d204f179e24a2607b9bfb2383374ed9137aa642) - support for EGM08 in `atl03x`

* [a72bb0a](https://github.com/SlideRuleEarth/sliderule/commit/a72bb0aa0724460bb6a5c3c5853fe938911bdb88) - added track information to GEDI dataframes

## Issues Resolved

* [8814ffc](https://github.com/SlideRuleEarth/sliderule/commit/8814ffc082ca5cd8368b74ce445cc02a8f3bb269) - CMR max resources reached returns error instead of silently truncating (matches client behavior)

* [97a55ae](https://github.com/SlideRuleEarth/sliderule/commit/97a55ae9bba78a1deb14f9da401e3b18eacd0626) - fixed access to BlueTopo

* [5a04947](https://github.com/SlideRuleEarth/sliderule/commit/5a049472402afb758b2a19227c6c3ae2aa5a5d5a) - fixed access to Meta Global Canopy

* [db38b4c](https://github.com/SlideRuleEarth/sliderule/commit/db38b4cd08f3f811497c7ffcca58f2befd60ff28) - s3 retries always rebuild header

* [db38b4c](https://github.com/SlideRuleEarth/sliderule/commit/db38b4cd08f3f811497c7ffcca58f2befd60ff28) - the endpoint being called is now provided in the metadata of the return dataframe

* [5b95dc4](https://github.com/SlideRuleEarth/sliderule/commit/5b95dc4979fc80b1aba7421e922b19e8a3d916f9) - more robust error handling in metric gathering in orchestrator

* [d8a813a](https://github.com/SlideRuleEarth/sliderule/commit/d8a813a99fa6da37c202c5662c99491425e7e1f6) - fixed GEDI raster sampling code to apply vertical offset when necessary

## Development Changes

* Overhauled and cleaned up the Python client examples and utilities.  They should all work; and the examples should provide a good starting point for using different features of the client.

* Python client reworked to pull out the code that handles the low-level information exchange protocol between the server and client into its own module `session.py`.  This **should** be transparent to users of the client; but makes the code easier to maintain, and supports future use cases where interaction with the server APIs wants to be handled directly.

* [db38b4c](https://github.com/SlideRuleEarth/sliderule/commit/db38b4cd08f3f811497c7ffcca58f2befd60ff28) - pytests do not jam DNS but wait for the cluster to fully come up including DNS entries to become active

* Static website overhauled with changes to the organization of the documentation, and a scrub of old information to bring it all up to date.

* GitHub actions / selftests / pytests - all updated to run cleanly

* [1840282](https://github.com/SlideRuleEarth/sliderule/commit/184028277976d83fd8f2ead22494bb380cb4b8be) - parameterized the use of CRS's in the Python client

## Getting This Release

[https://github.com/SlideRuleEarth/sliderule/releases/tag/v4.13.0](https://github.com/SlideRuleEarth/sliderule/releases/tag/v4.13.0)

## Benchmarks
> clients/python/utils/benchmark.py
```
atl06_aoi <690326 x 16> - 39.621595 secs
atl06_ancillary <914 x 17> - 2.990283 secs
atl03_ancillary <914 x 17> - 2.724033 secs
atl06_parquet <1577 x 18> - 2.700799 secs
atl03_parquet <22833 x 23> - 1.515308 secs
atl06_sample_landsat <914 x 20> - 8.168051 secs
atl06_sample_zonal_arcticdem <1696 x 27> - 8.840394 secs
atl06_sample_nn_arcticdem <1696 x 20> - 4.531418 secs
atl06_msample_nn_arcticdem <1696 x 20> - 4.470971 secs
atl06_no_sample_arcticdem <1696 x 16> - 2.554142 secs
atl03_rasterized_subset <51968 x 22> - 2.303672 secs
atl03_polygon_subset <50799 x 22> - 1.909250 secs
```

## Baseline
> clients/python/utils/baseline.py
```
GEDI / 3DEP = 2660.2507194650643
ICESat-2 / ArcticDEM = 1603.1273510971787
ICESat-2 / ATL06p = 1810.051657043249
ICESat-2 / PhoREAL = 4.078258037567139
```