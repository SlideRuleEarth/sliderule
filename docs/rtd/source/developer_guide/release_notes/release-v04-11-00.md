# Release v4.11.x

2025-04-01

Version description of the v4.11.1 release of SlideRule Earth.

## Major Changes

* **v4.13.0** - The official release of the SlideRule Web Client at https://client.slideruleearth.io

* **v4.13.0** - The `atl03x` endpoint is being previewed.  This implements a dataframe model for the data instead of a streaming model.

* **v4.13.0** - The `atl24x` endpoint provides subsetting support for the ATL24 standard data product.

## Issues Resolved

* **v4.13.1** - [a252b03](https://github.com/SlideRuleEarth/sliderule/commit/a252b03493c3128d5d5f491ced997366b3e0bf39) - earth data TNM interface returns diagnostic information when request fails

* **v4.13.1** - [b4a2ed2](https://github.com/SlideRuleEarth/sliderule/commit/b4a2ed2cd5ad923ee6d5dec7fad232387a8c25cd) - fixed handling of CMR request parameters

* **v4.13.1** - [b4a2ed2](https://github.com/SlideRuleEarth/sliderule/commit/b4a2ed2cd5ad923ee6d5dec7fad232387a8c25cd) - fixed bug in YAPC score column when version 0 was requested

* **v4.13.1** - [b4a2ed2](https://github.com/SlideRuleEarth/sliderule/commit/b4a2ed2cd5ad923ee6d5dec7fad232387a8c25cd) - fixed bathy parms selftest

* **v4.13.0** - [043a39b](https://github.com/SlideRuleEarth/sliderule/commit/043a39b9283119530dafebe77c7aabe61a7b58c0) - removed check in h5coro on compressed buffer size; removed atl24g check on number of beams

## Known Issues and Remaining Tasks

* The website at slideruleearth.io is undergoing an overhaul.

## Getting This Release

[https://github.com/SlideRuleEarth/sliderule/releases/tag/v4.11.1](https://github.com/SlideRuleEarth/sliderule/releases/tag/v4.11.1)
