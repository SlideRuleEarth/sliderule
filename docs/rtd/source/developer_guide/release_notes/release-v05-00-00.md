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

* ATL24 uses release 002 by default.

* Added the `3dep1m` asset which accesses the same USGS 3DEP data product but uses the internal AMS service for STAC queries.  This is an attempt to alleviate issues with inconsistent availability and functionality in The National Map (TNM) service which made using 3DEP difficult.

## Issues Resolved

* The latest SlideRule Conda-Forge package is now correctly installed when using the most updated conda resolver.

## Development Changes


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