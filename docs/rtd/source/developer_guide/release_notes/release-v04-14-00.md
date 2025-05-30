# Release v4.14.0

2025-05-09

Version description of the v4.14.0 release of SlideRule Earth.

## New/Improved Functionality

* Arbitrary Code Execution - `/source/ace` API for executing user supplied lua scripts; only available on private clusters.

* Asset Metadata Service - `/manager/ams` API for querying metadata directly from SlideRule; only ATL13 currently supported.

* ATL13 - `/source/atl13x` API for subsetting the ATL13 standard data product; in addition to normal temporal/spatial subsetting requests, SlideRule also supports subsetting based on the ATL13 reference ID, lake name, and coordinate within a body of water.

## Issues Resolved

* [9cea177](https://github.com/SlideRuleEarth/sliderule/commit/9cea1774194880b177a7d5781b0c24f316777007) - fixed s3 config permissions for writing out manager database

* [a9b875b](https://github.com/SlideRuleEarth/sliderule/commit/a9b875bc09d4ceb45ba4dc77de7271203a7f546f) - fixed bug in atl24 class_ph filter

* [#495](https://github.com/SlideRuleEarth/sliderule/issues/495) - fixed bug in x-series apis when some beams were empty

* [#496](https://github.com/SlideRuleEarth/sliderule/issues/496) - fixed multiple error handling bugs in h5coro discovered by @alma-pi

## Development Changes

## Getting This Release

[https://github.com/SlideRuleEarth/sliderule/releases/tag/v4.14.0](https://github.com/SlideRuleEarth/sliderule/releases/tag/v4.14.0)

## Benchmarks
> clients/python/utils/benchmark.py
```
```

## Baseline
> clients/python/utils/baseline.py
```
```