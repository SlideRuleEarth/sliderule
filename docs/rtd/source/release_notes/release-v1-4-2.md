# Release v1.4.2

2022-05-16

Version description of the v1.4.2 release of ICESat-2 SlideRule.

> :warning: This is the first release of the client to default to version '005' of the ICESat-2 ATL03 dataset.

## New Features

- Updated YAPC algorithm to Jeff's latest specification (05-04-22).  The new algorithm is the default version that runs.  If the previous algorithm is desired, there is a `version` parameter which is a part of the `yapc` parameter block that can be set to "1", and the original algorithm will execute.  Note: the new algorithm runs about three times faster than the original one.

- Updated internal threading hueristic in the Python client:
  * client will attempt to throttle the number of concurrent requests to any given processing node
  * the ***max_workers*** parameter in the `atl06p` and `atl03sp` APIs has been removed;  if the calling application must change the number pending requests per node, then there is a new API `sliderule.set_max_pending` that can be called.

- The `sliderule-docs` repository is now public and contains all of the SlideRule documentation used to build the http://icesat2sliderule.org website.

- API reference documentation has been moved from Sphinx into the python function docstrings; Sphinx now uses `.. autofunction::` to pull it into the website.

- SlideRule's Python bindings (`srpybin`) can now be built with the ICESat-2 plugin support (this enables the H5Coro to read from the Cumulus S3 archives).

## Major Issues Resolved

- Fixed spot calculation to match what is specified in the ATL03 ATBD. [b8ea34b](https://github.com/ICESat2-SlideRule/sliderule-python/commit/b8ea34bb24ea243b968de5df8f12939857393f46).

- Fixed Voila demo points showing up 360 degrees away [#97](https://github.com/ICESat2-SlideRule/sliderule-python/pull/97)

- Added significant tests to pytest framework for client: algorithm, atl08, atl06gs, h5 failure cases

- A logging level is specified for Lua endpoints; this allows the `prometheus` and `health` endpoints to use the DEBUG log level so that the logs are not spammed.

## Minor Issues Resolved

- Multiple lua script bugs fixed (discovered mostly through static analysis, but some were discovered through runtime errors)

- Optimized `List.h` template sort function

- More robust error checking in `netsvc` package _post_ and _get_ functions (the http response code is checked explicitly).

- Fixed CPU usage metrics in Grafana [3a5fd92](https://github.com/ICESat2-SlideRule/sliderule-project/commit/3a5fd92178c3028c1fcd5d6b3c08ff0e2fff1758)

## Getting This Release

[https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v1.4.2](https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v1.4.2)

[https://github.com/ICESat2-SlideRule/sliderule-icesat2/releases/tag/v1.4.2](https://github.com/ICESat2-SlideRule/sliderule-icesat2/releases/tag/v1.4.2)

[https://github.com/ICESat2-SlideRule/sliderule-python/releases/tag/v1.4.2](https://github.com/ICESat2-SlideRule/sliderule-python/releases/tag/v1.4.2)

