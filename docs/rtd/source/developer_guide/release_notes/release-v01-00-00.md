# Release v1.0.x

2021-06-29

Version description of the v1.0.7 release of ICESat-2 SlideRule

* **v1.0.7** - Resolved background density calculation issue: [sliderule#2](https://github.com/SlideRuleEarth/sliderule/issues/2)

* **v1.0.7** - Resolved background rate interpolation issue: [sliderule#3](https://github.com/SlideRuleEarth/sliderule/issues/3)

* **v1.0.7** - Resolved spacecraft velocity calculation issue: [sliderule#5](https://github.com/SlideRuleEarth/sliderule/issues/5)

* **v1.0.7** - Resolved along-track distance calculation issue: [sliderule#73](https://github.com/SlideRuleEarth/sliderule/issues/73)

* **v1.0.7** - ATL06-SR segment IDs are calculated by finding the center of the segment and using the segment ID of the closest ATL06 segment: [sliderule#72](https://github.com/SlideRuleEarth/sliderule/issues/72)

* **v1.0.7** - Resolved consul exit functionality regression: devops#42

* **v1.0.7** - Added health checking for SlideRule server nodes, with automatic restarts: devops#43
  * rate of once per minute
  * 3 retries: 5 seconds, 10 seconds, 30 seconds
  * unhealthy: connection failure, or health endpoint returns false

* **v1.0.6** - Added support for NSIDC/Cumulus data in AWS US-West-2
  * New [netsvc](https://github.com/SlideRuleEarth/sliderule/tree/main/packages/netsvc) package that uses `libcurl` for making https requests.
  * New capability to "fork" a Lua script [2385b99](https://github.com/SlideRuleEarth/sliderule/commit/2385b99bb80bd20aabbf83b357b9fbf5d088f740)
  * Reworked internal handling of assets; an asset now defines an I/O driver which must be registered prior to an asset being requested.
  * New capability to fetch files from S3 directly from a Lua script using `aws.s3get`
  * Created CredentialStore module in `aws` package for managing AWS credentials: [CredentialStore.cpp](https://github.com/SlideRuleEarth/sliderule/blob/a86b719b429c75289234c00d968cd9946c8710de/packages/aws/CredentialStore.cpp)
  * Created authentication script (written in lua) that runs as a daemon and maintains up-to-date AWS credentials
  * Fixed and enhanced TimeLib to support AWS time formats

* **v1.0.6** - Added support for metric collection along with a `metric` endpoint and associated Python client utility [query_metrics.py](https://github.com/SlideRuleEarth/sliderule-python/blob/50f64a9039630cb8390345c87110adab08ded79f/utils/query_metrics.py)

* **v1.0.6** - Added support for tailing server logs remotely via the `tail` endpoint and associated Python client utility [tail_events.py](https://github.com/SlideRuleEarth/sliderule-python/blob/50f64a9039630cb8390345c87110adab08ded79f/utils/tail_events.py)

* **v1.0.6** - Added native HttpClient module in `core` package: [HttpClient.cpp](https://github.com/SlideRuleEarth/sliderule/blob/a86b719b429c75289234c00d968cd9946c8710de/packages/core/HttpClient.cpp)

* **v1.0.6** - Added support for building on CentOS 7 [1a728b9](https://github.com/SlideRuleEarth/sliderule/commit/1a728b919dca2d260f1166bcd73dec1a37821d7c)

* **v1.0.5** - Increased the maximum number of points in a polygon (region of interest) the server supports from 32 to 16384.

* **v1.0.5** - Optimized point inclusion code, and added iterator subclass to list template.

* **v1.0.5** - Added logic to python client to iteratively simplify complex polygons until CMR accepts the query.

* **v1.0.4** - The ATL06 response records now include the `h_sigma` field ([833a3bd](https://github.com/SlideRuleEarth/sliderule-icesat2/commit/833a3bd7feca8deb77b7671ef96b0adfda07a48c))

* **v1.0.4** - Updated H5Coro to support version 4 ATL03 files ([6198cec](https://github.com/SlideRuleEarth/sliderule/commit/6198cec550268ca66784dd2c3b0be1a53f94d6bf))

* **v1.0.4** - Fixed symbol table bug in H5Coro ([f80857c](https://github.com/SlideRuleEarth/sliderule/commit/f80857ce83d73d6ec3ba71083de6981f3570c95a))

* **v1.0.4** - Resource names are sanitized before being used ([61f768c](https://github.com/SlideRuleEarth/sliderule/commit/61f768c825bc41074fdc20ff1269128f29bffb47))

* **v1.0.4** - Renamed ***h5coro-python*** target to ***binding-python***, and it is now treated as Python bindings for SlideRule.  Python bindings are not intended to become a Python package.

* **v1.0.4** - Created Python utility for displaying version of SlideRule running ([query_version.py](https://github.com/SlideRuleEarth/sliderule-python/blob/main/utils/query_version.py))

* **v1.0.4** - `icesat2.toregion` api now supports GeoJSON, Shapefiles, and simplifying polygons (see [documentation](/rtd/user_guide/ICESat-2.html#toregion))

* **v1.0.4** - Created `sliderule-project` repository for icesat2sliderule.org; all documentation moved to this repository (includes Read the Docs).

* **v1.0.4** - Optimized `icesat2.h5` api by using _bytearray_ when concatenating responses ([da05da6](https://github.com/SlideRuleEarth/sliderule-python/commit/da05da6a272e7decc75b75bf11716be383cb53d7))

* **v1.0.3** - The ATL06 response records now include `n_fit_photons`, `rms_misfit`, and `w_surface_window_final` fields.

* **v1.0.3** - There is now a default upper limit on the number of granules that can be processed by a single request

* **v1.0.3** - There is a dedicated ATL03 endpoint `atl03s` that will return segmented photon data within a polygon

* **v1.0.3** - A `version` endpoint was added that returns version information for the running system (things like version number, commit id, packages loaded, launch time)

* **v1.0.3** - The `h5p` endpoint was added to support reading multiple datasets concurrently from a single h5 file

## Known Issues

* **v1.0.6** -  Consul-Exit is not working and therefore a node does not disappear from the service group when it goes down.

## Getting this release

[https://github.com/SlideRuleEarth/sliderule/releases/tag/v1.0.7](https://github.com/SlideRuleEarth/sliderule/releases/tag/v1.0.7)

[https://github.com/SlideRuleEarth/sliderule-icesat2/releases/tag/v1.0.7](https://github.com/SlideRuleEarth/sliderule-icesat2/releases/tag/v1.0.7)

[https://github.com/SlideRuleEarth/sliderule-python/releases/tag/v1.0.7](https://github.com/SlideRuleEarth/sliderule-python/releases/tag/v1.0.7)
