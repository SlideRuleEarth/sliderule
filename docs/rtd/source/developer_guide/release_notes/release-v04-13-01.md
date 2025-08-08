# Release v4.13.1

2025-05-09

Version description of the v4.13.1 release of SlideRule Earth.

## New/Improved Functionality

* [#466](https://github.com/SlideRuleEarth/sliderule/issues/466) - source dataset information embedded into results

* [#493](https://github.com/SlideRuleEarth/sliderule/issues/493) - GEDI results include orbit number

## Issues Resolved

* [#266](https://github.com/SlideRuleEarth/sliderule/issues/266) - GEDI rasters support vertical shifts

* [#487](https://github.com/SlideRuleEarth/sliderule/issues/487) - `atl24x` queries CMR for ATL24 dataset

* [#463](https://github.com/SlideRuleEarth/sliderule/issues/463) - YAPC version 3 fixed

## Development Changes

* [#222](https://github.com/SlideRuleEarth/sliderule/issues/222) - removed jamming dns for github actions; instead just wait for dns to be active

* [#486](https://github.com/SlideRuleEarth/sliderule/issues/486), [#212](https://github.com/SlideRuleEarth/sliderule/issues/212) - logs backups now rely on manager

* [#140](https://github.com/SlideRuleEarth/sliderule/issues/140) - build version information includes OS info

* [#492](https://github.com/SlideRuleEarth/sliderule/issues/140) - updating signature of override of dns lookup in python client to interface with dask without issues

* [#418](https://github.com/SlideRuleEarth/sliderule/issues/418), [#56](https://github.com/SlideRuleEarth/sliderule/issues/56) - reworked runtime configuration management

* [#480](https://github.com/SlideRuleEarth/sliderule/issues/480) - scrubbed header files for faster compile times

## Getting This Release

[https://github.com/SlideRuleEarth/sliderule/releases/tag/v4.13.1](https://github.com/SlideRuleEarth/sliderule/releases/tag/v4.13.1)

## Benchmarks
> clients/python/utils/benchmark.py
```
atl06_aoi <709044 x 16> - 35.993670 secs
atl06_ancillary <914 x 17> - 3.651047 secs
atl03_ancillary <914 x 17> - 2.632388 secs
atl06_parquet <1577 x 18> - 3.057994 secs
atl03_parquet <22833 x 23> - 1.528023 secs
atl06_sample_landsat <914 x 20> - 8.521054 secs
atl06_sample_zonal_arcticdem <1696 x 27> - 6.279767 secs
atl06_sample_nn_arcticdem <1696 x 20> - 4.667313 secs
atl06_msample_nn_arcticdem <1696 x 20> - 4.434570 secs
atl06_no_sample_arcticdem <1696 x 16> - 2.425868 secs
atl03_rasterized_subset <51968 x 22> - 3.300175 secs
atl03_polygon_subset <50799 x 22> - 1.937227 secs
```

## Baseline
> clients/python/utils/baseline.py
```
GEDI / 3DEP = 2660.2507194650643
ICESat-2 / ArcticDEM = 1603.1273510971787
ICESat-2 / ATL06p = 1810.051657043249
ICESat-2 / PhoREAL = 4.078258037567139
```