# Release v1.1.x

2021-10-13

Version description of the v1.1.5 release of ICESat-2 SlideRule.

## New Features

* **v1.1.5** - Default asset switched from project's s3 bucket (atlas-s3) to NSIDC's Cumulus bucket (nsidc-s3).

* **v1.1.5** - Non-blocking read API added to H5Coro to support greater parallelization of reads.  When configured, H5Coro creates a thread-pool, assigns read requests to the next available thread, and returns a future to the calling application. The Atl03Reader in ICESat-2 plugin was updated to use the non-blocking API.  Concurrent reads when from a maximum of 18 reads at a time to 128 reads at a time.  A roughly 30% performance improvement was measured as a result of this change.

* **v1.1.5** - HttpServer (via the LuaEndpoint module) now monitors memory usage on the local system and will return a 503 response to any streaming requests made when the current memory usage exceeds a preconfigured threshold.

* **v1.1.5** - Test framework (pytest) and GitHub actions added to sliderule-python repository: [#56](https://github.com/SlideRuleEarth/sliderule-python/pull/56)

* **v1.1.5** - Polygon information extracted from icepyx region when using the ipxapi.py APIs: [#60](https://github.com/SlideRuleEarth/sliderule-python/pull/60)

* **v1.1.3** - In the `atl03rec` record (icesat2.atl03s and icesat2.atl03sp APIs), the `info` field has been replaced by the `atl08_class` and `atl03_cnf` fields which hold the ATL08 photon classification, and ATL03 photon confidence level respectively.

* **v1.1.3** - The default confidence level for all APIs that accept a confidence level has been changed to background (0) from high (4).

* **v1.1.2** - The api_widgets_demo updated to provide interface for creating polygon/bounding boxes, and to select a land class.

* **v1.1.1** - Added `ipxapi.py` module to Python client to support ICEPyx users.

* **v1.1.1** - GeoDataFrames are now sorted by time. Time is also used as the index. (APIs affected: `atl06`, `atl06p`, `atl03s`, `atl03sp`).

* **v1.1.0** - ATL08 classifications are now supported in the `atl06`, `atl06p`, `atl03s`, `atl03sp` APIs: [sliderule#71](https://github.com/SlideRuleEarth/sliderule/issues/71)
  * when the request parameters supply a list of ATL08 classifications to use, the server code will read the corresponding ATL08 data and only use the supplied classifications in the calculation
  * the following classifications are supported: noise, ground, canopy, top of canopy, unclassified
  * for the `atl03s`, `atl03sp`, the presence of the ATL08 classification list also enables the returned photon data to include each photons classification

* **v1.1.0** - The following APIs now return GeoDataFrames instead of dictionaries: `atl06`, `atl06p`, `atl03s`, `atl03sp`.
  * this standardizes the return structure at no cost to performance
  * each GeoDataFrame has a _"time"_ column which is a Python `datetime` value
  * each GeoDataFrame uses the ***geometry.x*** and ***geometry.y*** to represent the _"longitude"_ and _"latitude"_ fields respectively.
  * the *"delta_time"* column now represents the time from the ATLAS Standard Data Product (SDP) epoch (January 1, 2018)
  * The GeoDataFrames returned by `atl03s` and `atl03sp` contain a row for each photon that is returned

* **v1.1.0** - All APIs default to using version 004 of the data products.

* **v1.1.0** - Added the ground track field (_"gt"_) to the `atl06` and `atl06p` elevation returns.
  * added the following constants to the **icesat2.py** module: GT1L, GT1R, GT2L, GT2R, GT3L, GT3R
  * you can now do things like:
    ```python
    new_gdf = gdf[gdf["gt"] == icesat2.GT1L]
    ```

* **v1.1.0** - Added STRONG_SPOTS and WEAK_SPOTS constants to the **icesat2.py** module
  * you can no do things like:
    ```python
    new_gdf = atl06[atl06["spot"].isin(icesat2.WEAK_SPOTS)]
    ```

* **v1.1.0** - Python client published to conda-forge: [sliderule](https://anaconda.org/conda-forge/sliderule)

## Major Issues Resolved

* **v1.1.5** - Unexpected termination of response data is now gracefully handled in the client with the request being retried: [#40](https://github.com/SlideRuleEarth/sliderule-python/issues/40)

* **v1.1.5** - Client updated to handle server-side memory issues gracefully - worker scale factor reduced to 3, back off (via sleep function) added to error handling, explicit handling of 503 error response: [c47f0f0](https://github.com/SlideRuleEarth/sliderule-python/commit/c47f0f02a5142fe7cf19a440910d60f4e7adee70)

* **v1.1.4** - The XMIT_SIGMA parameter of the ATL06 algorithm was updated to the correct value. [cc47c5f](https://github.com/SlideRuleEarth/sliderule-icesat2/commit/cc47c5f49914238279a6b1e045dfe8672b26ffc7)

* **v1.1.2** - H5Coro memory usage was optimized to support runs against larger datasets

* **v1.1.1** - The issue with the server crashing in version 1.1.0 was resolved.
  - In version v1.0.6, support for NSIDC Cumulus was added; but it had a dormant bug in the implementation.
  - Each server maintains up-to-date AWS credentials for NSIDC resources in S3 by authenticating to the earthdata login servers and getting API tokens that last for 1 hour.  Every 30 minutes, each server re-authenticates and gets new tokens.  For performance reasons, AWS clients are pre-allocated and re-used.  The AWS clients are only cycled when new tokens arrive.  The code that created new clients with the updated tokens had race-conditions in it where a deleted client could still be in use.
  - The introduction of ATL08 processing in version 1.1.0 lengthed the amount of time a client was in use and therefore made the race-condition much more likely to occur.
  - The race condition has been fixed in this version.

* **v1.1.0** - Resolved delta_time calculation issue: [sliderule#48](https://github.com/SlideRuleEarth/sliderule/issues/48)

* **v1.1.0** - Resolved latitude and longitude calculation issue: [sliderule#7](https://github.com/SlideRuleEarth/sliderule/issues/7)

* **v1.1.0** - HttpServer class fatal exception bug that causes server to hang was fixed: [sliderule#77](https://github.com/SlideRuleEarth/sliderule/issues/77)

* **v1.1.0** - Health check made more robust to handle case when server connection hangs: [devops#45](https://github.com/SlideRuleEarth/devops/issues/45)

## Minor Issues Resolved

* **v1.1.5** - HSDS support removed from codebase

* **v1.1.5** - Fixed count returned in Grand Mesa demo: [#42](https://github.com/SlideRuleEarth/sliderule-python/issues/42)

* **v1.1.5** - Ipyleaflet updates and fixes in io.py and ipysliderule.py modules: [#54](https://github.com/SlideRuleEarth/sliderule-python/pull/54)

* **v1.1.5** - PyPI dependencies listed in requirements.txt reduced to minimal set: [#57](https://github.com/SlideRuleEarth/sliderule-python/pull/57)

* **v1.1.5** - Fixed typo in CMR time query: [1e77117](https://github.com/SlideRuleEarth/sliderule-python/commit/1e77117b0d4adbac0c657a4a4890a587cd4b5509)

* **v1.1.5** - Fixed geometry warning in creation of geodataframe: [#52](https://github.com/SlideRuleEarth/sliderule-python/issues/52)

* **v1.1.3** - An out-of-bounds vulnerability was fixed in the processing of ATL08 photon classifications. [658409c](https://github.com/SlideRuleEarth/sliderule-icesat2/commit/658409cb1fac8608b0489e40cfd772cae2b66a01)

* **v1.1.2** - GeoDataFrame index (which is a datetime calculated from the delta_time column) is now set to ns resolution so that all rows are unique. [ef2abce](https://github.com/SlideRuleEarth/sliderule-python/commit/ef2abce6d406cb1865c78ce6e0380063263c2336)

* **v1.1.2** - Fixed multiple memory leaks in h5coro, in the use of an asset object in reader classes, and in various other modules throughout the server code.

* **v1.1.2** - Fixed startup bug where client that needs credentials would never be able to get those credentials if created before the first set of credentials were available. [da139e6](https://github.com/SlideRuleEarth/sliderule/commit/da139e6fc395ed833a7ddf764ea9b3967385f6b3)

* **v1.1.1** - ATL08 classification can be supplied as a table or a string. [04bc5fb](https://github.com/SlideRuleEarth/sliderule-icesat2/commit/04bc5fbd8453517043156c04ba0d8fdaee011f48)

* **v1.1.1** - Fixed out of bounds access caught by valgrind. [5af94a5](https://github.com/SlideRuleEarth/sliderule-icesat2/commit/5af94a5e6c8266bd05d9bffe2dedf265c278ca62)

* **v1.1.1** - Changed name of Python bindings to `srpybin` to avoid naming conflicts with the SlideRule Python client. [0525c0b](https://github.com/SlideRuleEarth/sliderule/commit/0525c0b8ff32d761c6d8075c6fc16a66639cf80a)

* **v1.1.1** - Signed integer support added to Python bindings [sliderule#84](https://github.com/SlideRuleEarth/sliderule/issues/84)

* **v1.1.1** - Fixed bugs in s3cache I/O driver for H5Coro, and added support in Python bindings. [0170be8](https://github.com/SlideRuleEarth/sliderule/commit/0170be8e579bbde15a54a2e9b5b754b57657d90f)

* **v1.1.0** - TCP/IP and HTTP errors are caught and user-friendly error messages are printed: [sliderule#26](https://github.com/SlideRuleEarth/sliderule/issues/26)

* **v1.1.0** - Memory leak in Asset class fixed: [sliderule#32](https://github.com/SlideRuleEarth/sliderule/issues/32)

* **v1.1.0** - Server generated log messages logged by the Python client are logged at the level specified in the server log message: [sliderule-python#1](https://github.com/SlideRuleEarth/sliderule-python/issues/1)

* **v1.1.0** - The Python client logging is now completely turned off if the verbose setting is set to False: [sliderule-python#26](https://github.com/SlideRuleEarth/sliderule-python/issues/26)

* **v1.1.0** - Fixed bug in code where the along track spread and the number of photon checks were not being made in the correct place in the code.  In certain circumstances the final elevation could have been calculated on the set of photons that failed validation instead of the preceding set that did not fail the validation.  [cb5948c](https://github.com/SlideRuleEarth/sliderule-icesat2/commit/cb5948c8587f841ad6ade758bfeacc4f698786cc)

* **v1.1.0** - CMake build fails configuration if dependency not found: [sliderule#40](https://github.com/SlideRuleEarth/sliderule/issues/40)

* **v1.1.0** - Signals issued while mutex held in MsgQ class code: [sliderule#79](https://github.com/SlideRuleEarth/sliderule/issues/79)

* **v1.1.0** - Fixed race conditions in SlideRule's Python bindings; specifically fixed the H5Coro API.

* **v1.1.0** - Added `set_rqst_timeout` API to ***icesat2.py*** module to allow user to control what the timeouts are for connecting to and reading from SlideRule servers.

## Getting this release

[https://github.com/SlideRuleEarth/sliderule/releases/tag/v1.1.5](https://github.com/SlideRuleEarth/sliderule/releases/tag/v1.1.5)

[https://github.com/SlideRuleEarth/sliderule-icesat2/releases/tag/v1.1.5](https://github.com/SlideRuleEarth/sliderule-icesat2/releases/tag/v1.1.5)

[https://github.com/SlideRuleEarth/sliderule-python/releases/tag/v1.1.5](https://github.com/SlideRuleEarth/sliderule-python/releases/tag/v1.1.5)
