# Release v5.4.x

2025-05-08

Version description of the v5.4.1 release of SlideRule Earth.

## Significant Changes

* **v5.4.0** - The `arrow` endpoints are no longer routed from `/arrow/{endpoint}` but instead are routed from `/source/{endpoint}` with the `Accept: {content-type}` HTTP header supplied in the request.  The support arrow content types are _"application/arrow"_ or simply _"arrow"_.  For convenience, specifying the accepted content type of an endpoint can also be accomplished by supplying a suffix to the endpoint name: `/source/{endpoint}.{content-type}`.

For example the following are equivalent:
```
curl -H "Accept: arrow" https://sliderule.slideruleearth.io/source/atl03x
curl https://sliderule.slideruleearth.io/source/atl03x.arrow
```

## New Functionality

* **v5.4.0** - The SlideRule team is now tracking a coordinate per request that falls within the AOI of the request.

* **v5.4.1** [#608](https://github.com/SlideRuleEarth/sliderule/pull/608) - Generated OpenAPI specifications for all endpoints

## Issues Resolved

* **v5.4.0** - [95a2815](https://github.com/SlideRuleEarth/sliderule/commit/95a281598048850b536349ad06ac0e999a871bd3) - fixed ilb rate limiting regression introduced with user service capacity

## Development Updates

* **v5.4.0** - There was significant rework of internal code to support loading an endpoint and getting access to parameters and metadata prior to executing the endpoint.

## Getting This Release

[https://github.com/SlideRuleEarth/sliderule/releases/tag/v5.4.1](https://github.com/SlideRuleEarth/sliderule/releases/tag/v5.4.1)

## Metrics

> clients/python/utils/benchmark.py

> clients/python/utils/baseline.py
