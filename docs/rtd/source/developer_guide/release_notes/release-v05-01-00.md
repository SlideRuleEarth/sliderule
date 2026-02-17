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
```

> clients/python/utils/baseline.py
```
```