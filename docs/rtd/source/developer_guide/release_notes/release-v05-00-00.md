# Release v5.0.0

2025-12-02

Version description of the v5.0.0 release of SlideRule Earth.

## Breaking Changes

* Polygons used for `earthdata.stac` requests no longer need to be nested lists, but are supplied in the same format as all other requests:
```Python
poly = [
    {"lat": lat1, "lon": lon1},
    {"lat": lat2, "lon": lon2},
    ...
    {"lat": lat1, "lon": lon1}
]
```

* The `raster` field which was replaced by `region_mask` and has been deprecated is now no longer supported.

* The `nsidc-s3` asset which was replaced by the `icesat2` asset, is no longer supported.  Using `icesat2` instead of `nsidc-s3` provides identical behavior.

* The `bypass_dns` option in the SlideRule Python Client is no longer supported as it was not reliable.

* Queries to AMS require all parameters for the query to be inside an object with the key matching the product being queried.  For example, ATL13 queries can no longer provide the `refid` parameter at the top level of the request json, like so:
```json
{
    "refid": 110234231
}
```
Instead the request json must change to be this:
```json
{
    "atl13": {
        "refid": 110234231
    }
}
```

## Major Changes & New Functionality

* ATL24 uses release 002 by default, which uses the internal Asset Metadata Service (AMS).

* [#549](https://github.com/SlideRuleEarth/sliderule/pull/549) - `h5p` now supports slices.

* `earthdata.py` is no longer a standalone implementation of an interface to CMR and TNM, but instead makes a request to the SlideRule cluster to execute the server-side implementations in `earth_data_query.lua`.  This consolidates the interface to these services in one place, and also provides a consistent interface between the web and Python clients.

* Added the `3dep1m` asset which accesses the same USGS 3DEP data product but uses the internal AMS service for STAC queries.  This is an attempt to alleviate issues with inconsistent availability and functionality in The National Map (TNM) service which made using 3DEP difficult.

## Issues Resolved

* The latest SlideRule Conda-Forge package is now correctly installed when using the most updated conda resolver.

* [6a71ca6](https://github.com/SlideRuleEarth/sliderule/commit/6a71ca6c5a0a5cf7211a0f7da32473624d1bc5b0) - `atl03sp` endpoint throttled to prevent users from making large AOI requests and taking down servers.

* [fec547f](https://github.com/SlideRuleEarth/sliderule/commit/fec547f2c725b38cfecda9aafa787600b7fb17f5) - LAS/LAZ output will now open automatically in the Python client when `open_on_complete` is set to True.

* Rate limiting improved - error messages displayed in web client and python client; admin capabilities added for resetting rate limiting

## Development Changes

* [#545](https://github.com/SlideRuleEarth/sliderule/pull/545) - Substantial static analysis was performed on all packages using codex and issues were resolved.

* Runtime Exceptions (RTE) codes updated and reorganized to support the web client being able to provide meaningful feedback to user when an error occurs.

* [ed8c58f](https://github.com/SlideRuleEarth/sliderule/commit/ed8c58f33f4c447db69bc8ea52728fe09aeb47e0) - `free_func` removed from `msgq` design

* [57cbcd5](https://github.com/SlideRuleEarth/sliderule/commit/57cbcd582da57372c904e1e48d2071d2b0510848) - S3 puts provide better error messages in backend logs

* Docker containers are now all standardized on using AL2023 as the base image unless an alternative is required (e.g. ilb)

* [e982826](https://github.com/SlideRuleEarth/sliderule/commit/e9828262adff70ff5045f1e5e89512d4a0ce190f) - The base AMI used for all cluter nodes is not the AL2023 ECS optimized image provided by AWS.

## Getting This Release

[https://github.com/SlideRuleEarth/sliderule/releases/tag/v5.0.0](https://github.com/SlideRuleEarth/sliderule/releases/tag/v5.0.0)

## Benchmarks
> clients/python/utils/benchmark.py
```
```

## Baseline
> clients/python/utils/baseline.py
```
```