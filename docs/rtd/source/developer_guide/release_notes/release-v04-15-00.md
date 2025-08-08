# Release v4.15.0

2025-05-09

Version description of the v4.15.0 release of SlideRule Earth.

## Compatibility Changes

* The `h_mean` value in the `atl03x` API when running the ATL06 surface fitting algorithm was changed from a double to a float.  This was to make it consistent with the ATL06 standard data product and to normalize all DataFrames with *z* columns to floating point precision.

* The x-series APIs provide a different column for the sample time - `time_ns` instead of `time`.  This is to reflect that the new `time_ns` is provided as a Unix(ns) time, whereas the old `time` was provided as a GPS seconds time.  The Unix(ns) time makes it compatible with Pandas and easier to display for human readability.

## New/Improved Functionality

* NISAR sample dataset support

* 3DEP 10m dataset support

* Added `ams` endpoint to query the Asset Metadata Service from the SlideRule client.

* Added the `hdf` package which supports writing HDF5 files from the C++ SlideRule server code

* [5ba013b](https://github.com/SlideRuleEarth/sliderule/commit/5ba013b8ed7011ab55790f347c1dbdffcc449d95) - Added support for variable length data types to `h5coro`

## Issues Resolved

* [fc358ff](https://github.com/SlideRuleEarth/sliderule/commit/fc358ffb037c55a0d5427a6f1f313ce257280f17) - make `recordinfo` consistent in all parquet files.

* [5585d25](https://github.com/SlideRuleEarth/sliderule/commit/5585d252472ca27c379e211ff3068baec7d87d8a) - fixed bug preventing users from sampling `atl03x` original dataframe

* [911e8cc](https://github.com/SlideRuleEarth/sliderule/commit/911e8cc10f1b984663ac5571d3b058bfe85c7f5d) - fixed earthdata query for GEDI rasters

## Development Changes

* Support for no-downtime updates of the public cluster.  The provisioning system, for the time being, is not being used to update the public cluster.  Instead, we are using a green-blue deployment strategy where the new cluster is deployed and tested before the DNS record is updated to point slideruleearth.io to the new cluster.  Then after some time the old cluster is destroyed.

* Metric collection moved from orchestrator to manager (where they belong).

* Improved handling of duckdb database and generating usage reports

* [a127c4f](https://github.com/SlideRuleEarth/sliderule/commit/a127c4fccd3005513a0d6938c912cef445b926cc) - added z columns for all dataframes

* The SlideRule `buildenv` docker container can now be used to build and run the code in the release configuration.

* [3175f70](https://github.com/SlideRuleEarth/sliderule/commit/3175f70f19e9936fb8c4999046867b027de1814d) - static archive now includes all symbols

## Getting This Release

[https://github.com/SlideRuleEarth/sliderule/releases/tag/v4.15.0](https://github.com/SlideRuleEarth/sliderule/releases/tag/v4.15.0)

## Benchmarks
> clients/python/utils/benchmark.py
```
```

## Baseline
> clients/python/utils/baseline.py
:::{note}
:::
