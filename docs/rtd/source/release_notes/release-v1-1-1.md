# Release v1.1.1

2021-08-25

Version description of the v1.1.1 release of ICESat-2 SlideRule.

## New Features

* Added `ipxapi.py` module to Python client to support ICEPyx users.

* GeoDataFrames are now sorted by time. Time is also used as the index. (APIs affected: `atl06`, `atl06p`, `atl03s`, `atl03sp`).

## Major Issues Resolved

* The issue with the server crashing in version 1.1.0 was resolved.
  - In version v1.0.6, support for NSIDC Cumulus was added; but it had a dormant bug in the implementation.
  - Each server maintains up-to-date AWS credentials for NSIDC resources in S3 by authenticating to the earthdata login servers and getting API tokens that last for 1 hour.  Every 30 minutes, each server re-authenticates and gets new tokens.  For performance reasons, AWS clients are pre-allocated and re-used.  The AWS clients are only cycled when new tokens arrive.  The code that created new clients with the updated tokens had race-conditions in it where a deleted client could still be in use.
  - The introduction of ATL08 processing in version 1.1.0 lengthed the amount of time a client was in use and therefore made the race-condition much more likely to occur.
  - The race condition has been fixed in this version.

## Minor Issues Resolved

* ATL08 classification can be supplied as a table or a string. [04bc5fb](https://github.com/ICESat2-SlideRule/sliderule-icesat2/commit/04bc5fbd8453517043156c04ba0d8fdaee011f48)

* Fixed out of bounds access caught by valgrind. [5af94a5](https://github.com/ICESat2-SlideRule/sliderule-icesat2/commit/5af94a5e6c8266bd05d9bffe2dedf265c278ca62)


* Changed name of Python bindings to `srpybin` to avoid naming conflicts with the SlideRule Python client. [0525c0b](https://github.com/ICESat2-SlideRule/sliderule/commit/0525c0b8ff32d761c6d8075c6fc16a66639cf80a)

* Signed integer support added to Python bindings [sliderule#84](https://github.com/ICESat2-SlideRule/sliderule/issues/84)

* Fixed bugs in s3cache I/O driver for H5Coro, and added support in Python bindings. [0170be8](https://github.com/ICESat2-SlideRule/sliderule/commit/0170be8e579bbde15a54a2e9b5b754b57657d90f)

## Getting this release

[https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v1.1.1](https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v1.1.1)

[https://github.com/ICESat2-SlideRule/sliderule-icesat2/releases/tag/v1.1.1](https://github.com/ICESat2-SlideRule/sliderule-icesat2/releases/tag/v1.1.1)

[https://github.com/ICESat2-SlideRule/sliderule-python/releases/tag/v1.1.1](https://github.com/ICESat2-SlideRule/sliderule-python/releases/tag/v1.1.1)

