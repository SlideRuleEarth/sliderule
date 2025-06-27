# Release v.4.16.0

2025-06-27

Version description of the v.4.16.0 release of SlideRule Earth.

## New/Improved Functionality

* [3c4aa74](https://github.com/SlideRuleEarth/sliderule/commit/3c4aa74afe515b0306f77af9cec8d9effe8bc0fd) - `fields` parameter was added to the `output` parameters

* PGC ArcticDEM Strips and REMA Strips now use PGC STAC endpoint

* Rate limiting implemented on public cluster.  When a request location (defined as the registered location for the source IP address of the client originating the request) exceeds 100K granules processed in a given week, then IPs from that location are rate limited to processing 1 granule per minute for the rest of the week.  Requests that are rate limited are responded to with a 429 HTTP code and a corresponding `retry-after` header.

* [aba56e5](https://github.com/SlideRuleEarth/sliderule/commit/aba56e58f64a97427a03df936e24d358f32e582d) and [#563](https://github.com/SlideRuleEarth/sliderule-web-client/issues/563) - added `assets` endpoint

## Issues Resolved

* [e238acd](https://github.com/SlideRuleEarth/sliderule/commit/e238acd92d1745862325d2a96949a342e62b5250) - clarified confusing version mismatch warning

## Development Changes

* Pytests that depend on external services (CMR, STAC, TNM) are marked as such and do not cause a CI/CD pipeline failure.

* The _Usage Report_ now includes the time span over which the data is being summarized.

* Added node.js tests that simulate the browser environment to catch potential issues with browser compatibility

* Exposed `/discovery/status` endpoint to the web client (worked out CORS headers)

* [6d58b0d](https://github.com/SlideRuleEarth/sliderule/commit/6d58b0d106febc306b749123335a6c73b8e13ea5) - notebooks rendered as part of building the static website

* On startup, attempt is made to download the leap seconds file from NIST and use it instead of the one checked into the repository

## Getting This Release

[https://github.com/SlideRuleEarth/sliderule/releases/tag/v.4.16.0](https://github.com/SlideRuleEarth/sliderule/releases/tag/v.4.16.0)

## Benchmarks
> clients/python/utils/benchmark.py
```
```

## Baseline
> clients/python/utils/baseline.py
:::{note}
:::
