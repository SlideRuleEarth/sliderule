# Release v4.18.0

2025-08-06

Version description of the v4.18.0 release of SlideRule Earth.

## Major Changes

* SlideRule now defaults to using release `007` for applicable ICESat-2 Standard Data Products.

## Issues Resolved

* [#470](https://github.com/SlideRuleEarth/sliderule/issues/470) - fixed race condition in getting authentication token from provisioning system on a refresh

* [02ae4ee](https://github.com/SlideRuleEarth/sliderule/commit/02ae4ee2659dd29d767cd466adec42fff8f69cef) - added ability to override locks per request

* [ae1ec61](https://github.com/SlideRuleEarth/sliderule/commit/ae1ec61a3f066ea168fbad399523269275218c64) - fixed corner case bug when serializing dataframes and the number of rows in the dataframe is exactly divisible by the serialization chunk size.

* [#514](https://github.com/SlideRuleEarth/sliderule/issues/514) [#517](https://github.com/SlideRuleEarth/sliderule/issues/517) - endpoint and request string embedded in metadata of parquet output

* [#519](https://github.com/SlideRuleEarth/sliderule/issues/519) - zstd used by default for parquet output

* [d465493](https://github.com/SlideRuleEarth/sliderule/commit/d4654933553d6245544c021d27ebbda35c108fab) - fixed serious bugs in GEDI subsetting code related to the use of ancillary fields.

## Development Changes

* Updated available cluster configurations to focus more on the t-series instances and also support large low-cost clusters that maximum network bandwidth allocations (e.g. t4g.large instance type).

* Various documentation updates, mostly focusing on the Developer's Guide.

## Getting This Release

[https://github.com/SlideRuleEarth/sliderule/releases/tag/v4.18.0](https://github.com/SlideRuleEarth/sliderule/releases/tag/v4.18.0)

## Benchmarks
> clients/python/utils/benchmark.py
```
atl06_aoi <718584 x 16> - 31.416050 secs
atl06_ancillary <916 x 17> - 2.676247 secs
atl03_ancillary <916 x 17> - 2.466998 secs
atl06_parquet <1600 x 18> - 2.820411 secs
atl03_parquet <23072 x 23> - 1.526969 secs
atl06_sample_landsat <916 x 20> - 14.343591 secs
atl06_sample_zonal_arcticdem <1695 x 27> - 4.964642 secs
atl06_sample_nn_arcticdem <1695 x 20> - 4.572921 secs
atl06_msample_nn_arcticdem <1695 x 20> - 4.940101 secs
atl06_no_sample_arcticdem <1695 x 16> - 2.726807 secs
atl03_rasterized_subset <51832 x 22> - 2.466026 secs
atl03_polygon_subset <50615 x 22> - 2.450325 secs
```

## Baseline
> clients/python/utils/baseline.py
```
GEDI / 3DEP = 2660.2507194650643
ICESat-2 / ArcticDEM = 1598.2301327720206
ICESat-2 / ATL06p = 1809.8936901734673
ICESat-2 / PhoREAL = 3.245903730392456
```