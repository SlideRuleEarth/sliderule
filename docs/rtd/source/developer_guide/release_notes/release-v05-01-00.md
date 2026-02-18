# Release v5.1.x

2025-02-17

Version description of the v5.1.0 release of SlideRule Earth.


## New Functionality

* **v5.1.0** - [#575](https://github.com/SlideRuleEarth/sliderule/pull/575) - support for user supplied raster sampling; see [example](https://github.com/SlideRuleEarth/sliderule/blob/main/clients/python/examples/user_url_raster.ipynb) for usage details.

**v5.1.0** - _SlideRule Runner_ feature preview; provides batch job management for large processing runs; see [example](https://github.com/SlideRuleEarth/sliderule/blob/main/clients/python/examples/job_runner.ipynb) for usage details.

## Issues Resolved

* **v5.1.0** - Fixed `atl13` and `atl24` access example notebooks to use latest `earthdata` endpoint.

* **v5.1.0** - [b486378](https://github.com/SlideRuleEarth/sliderule/commit/b4863780d1c2f6319faa1329f8a1e340f1f4989c) - Fixed name filter support in AMS
## Development Updates

* **v5.1.0** - Updates to _usage report_ and cleanup of recorder to remove all API gateway components.

* **v5.1.0** - Selftest global cleanup to use `runner.unittest` framework for all tests; supports test tags and selective runs.

## Getting This Release

[https://github.com/SlideRuleEarth/sliderule/releases/tag/v5.1.0](https://github.com/SlideRuleEarth/sliderule/releases/tag/v5.1.0)

## Metrics

> clients/python/utils/benchmark.py
```
atl06_aoi <776718 x 16> - 32.487827 secs
atl06_ancillary <916 x 17> - 3.075466 secs
atl03_ancillary <916 x 17> - 2.546856 secs
atl06_parquet <1600 x 18> - 2.866104 secs
atl03_parquet <23072 x 23> - 1.625029 secs
atl06_sample_landsat <916 x 20> - 59.372218 secs
atl06_sample_zonal_arcticdem <1695 x 27> - 4.941877 secs
atl06_sample_nn_arcticdem <1695 x 20> - 4.959598 secs
atl06_msample_nn_arcticdem <1695 x 20> - 4.646932 secs
atl06_no_sample_arcticdem <1695 x 16> - 2.825487 secs
atl03_rasterized_subset <51832 x 22> - 2.469260 secs
atl03_polygon_subset <50615 x 22> - 2.040451 secs
```

> clients/python/utils/baseline.py
```
GEDI / 3DEP = 2653.7247077111642
ICESat-2 / ArcticDEM = 1598.2301327720206
ICESat-2 / ATL06p = 1809.893690173468
ICESat-2 / PhoREAL = 3.245903968811035
```