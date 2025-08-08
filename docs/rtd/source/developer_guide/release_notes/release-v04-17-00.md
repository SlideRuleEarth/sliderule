# Release v4.17.0

2025-07-17

Version description of the v4.17.0 release of SlideRule Earth.

## New/Improved Functionality

* [#511](https://github.com/SlideRuleEarth/sliderule/pull/511) - slope and aspect derivatives for sampled rasters

## Issues Resolved

* Fixed issues with rate limiting: only applied to public cluster, moved limit to 50K per week

* Fixed issues with Surface Fitter (the `atl03x` version of atl06-sr) and created a gold standard test to compare and track results against the Standard Data Product

* [031ebaf](https://github.com/SlideRuleEarth/sliderule/commit/031ebaf3932685fcf0451d816e021106005ff3d4) - code now uses CRS information for pixel size

* [#510](https://github.com/SlideRuleEarth/sliderule/issues/510) - fixed N/A values in grafana dashboards when metrics didn't exist

## Development Changes

* The Asset Metadata Service is not run as a container on every cluster node.  This is a better fit for the `ams` because it involves considerable processing power to subset the ATL13 data.  When a node processes an ATL13 request, it uses the `ams` running on the same node.

* The Asset Metadata Service now uses **duckdb** instead of **geopandas** for spatial subsetting.  **duckdb** uses less memory and has a faster startup time.  The query time is similar.

## Getting This Release

[https://github.com/SlideRuleEarth/sliderule/releases/tag/v4.17.0](https://github.com/SlideRuleEarth/sliderule/releases/tag/v4.17.0)

## Benchmarks
> clients/python/utils/benchmark.py
```
atl06_aoi <709044 x 16> - 36.257619 secs
atl06_ancillary <914 x 17> - 3.095431 secs
atl03_ancillary <914 x 17> - 3.136149 secs
atl06_parquet <1577 x 18> - 2.666372 secs
atl03_parquet <22833 x 23> - 1.527147 secs
atl06_sample_landsat <914 x 20> - 8.620751 secs
atl06_sample_zonal_arcticdem <1696 x 27> - 4.759872 secs
atl06_sample_nn_arcticdem <1696 x 20> - 4.668834 secs
atl06_msample_nn_arcticdem <1696 x 20> - 5.492570 secs
atl06_no_sample_arcticdem <1696 x 16> - 3.495109 secs
atl03_rasterized_subset <51968 x 22> - 2.369336 secs
atl03_polygon_subset <50799 x 22> - 2.186130 secs
```

## Baseline
> clients/python/utils/baseline.py
```
GEDI / 3DEP = 2660.2507194650643
ICESat-2 / ArcticDEM = 1603.1273510971787
ICESat-2 / ATL06p = 1810.051657043249
ICESat-2 / PhoREAL = 4.078258037567139
```