# Release v1.1.5

2021-10-13

Version description of the v1.1.5 release of ICESat-2 SlideRule.

## New Features

* Default asset switched from project's s3 bucket (atlas-s3) to NSIDC's Cumulus bucket (nsidc-s3).

* Non-blocking read API added to H5Coro to support greater parallelization of reads.  When configured, H5Coro creates a thread-pool, assigns read requests to the next available thread, and returns a future to the calling application. The Atl03Reader in ICESat-2 plugin was updated to use the non-blocking API.  Concurrent reads when from a maximum of 18 reads at a time to 128 reads at a time.  A roughly 30% performance improvement was measured as a result of this change.

* HttpServer (via the LuaEndpoint module) now monitors memory usage on the local system and will return a 503 response to any streaming requests made when the current memory usage exceeds a preconfigured threshold.

* Test framework (pytest) and GitHub actions added to sliderule-python repository: [#56](https://github.com/ICESat2-SlideRule/sliderule-python/pull/56)

* Polygon information extracted from icepyx region when using the ipxapi.py APIs: [#60](https://github.com/ICESat2-SlideRule/sliderule-python/pull/60)
## Major Issues Resolved

* Unexpected termination of response data is now gracefully handled in the client with the request being retried: [#40](https://github.com/ICESat2-SlideRule/sliderule-python/issues/40)

* Client updated to handle server-side memory issues gracefully - worker scale factor reduced to 3, back off (via sleep function) added to error handling, explicit handling of 503 error response: [c47f0f0](https://github.com/ICESat2-SlideRule/sliderule-python/commit/c47f0f02a5142fe7cf19a440910d60f4e7adee70)

## Minor Issues Resolved

* HSDS support removed from codebase

* Fixed count returned in Grand Mesa demo: [#42](https://github.com/ICESat2-SlideRule/sliderule-python/issues/42)

* Ipyleaflet updates and fixes in io.py and ipysliderule.py modules: [#54](https://github.com/ICESat2-SlideRule/sliderule-python/pull/54)

* PyPI dependencies listed in requirements.txt reduced to minimal set: [#57](https://github.com/ICESat2-SlideRule/sliderule-python/pull/57)

* Fixed typo in CMR time query: [1e77117](https://github.com/ICESat2-SlideRule/sliderule-python/commit/1e77117b0d4adbac0c657a4a4890a587cd4b5509)

* Fixed geometry warning in creation of geodataframe: [#52](https://github.com/ICESat2-SlideRule/sliderule-python/issues/52)

## Getting This Release

[https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v1.1.5](https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v1.1.5)

[https://github.com/ICESat2-SlideRule/sliderule-icesat2/releases/tag/v1.1.5](https://github.com/ICESat2-SlideRule/sliderule-icesat2/releases/tag/v1.1.5)

[https://github.com/ICESat2-SlideRule/sliderule-python/releases/tag/v1.1.5](https://github.com/ICESat2-SlideRule/sliderule-python/releases/tag/v1.1.5)

