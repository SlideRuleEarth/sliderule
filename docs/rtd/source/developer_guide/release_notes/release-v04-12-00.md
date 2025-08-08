# Release v4.12.0

2025-04-01

Version description of the v4.12.0 release of SlideRule Earth.

## New Functionality

* [f59dadd](https://github.com/SlideRuleEarth/sliderule/commit/f59dadd6f36652d3ca8b30fed07d2d50ab494095) - Initial support for GEDTM raster sampling added (`gedtm-30meter`, `gedtm-std`, `gedtm-dfm`).

* Added `/arrow` endpoint route to support raw streaming of arrow supported files (does not use the SlideRule native protocol)

* [159563e](https://github.com/SlideRuleEarth/sliderule/commit/159563e8422e33a932b5524151259c8181e0deb7) - Added examples for using cURL and the `/arrow` routed endpoints

## Issues Resolved

* [5421f61](https://github.com/SlideRuleEarth/sliderule/commit/5421f6187c267c31e3fc3e06002bb07c1f296ecb) - we now allow zero-length dataframes to be returned; we believe this is a more natural response to a user that makes a valid request for which there is no data

* [aaf66d5](https://github.com/SlideRuleEarth/sliderule/commit/aaf66d53d95fff6e3c5d3396689c362e88e467e4) - fixed CMR requests that don't use the `icesat2` asset

## Development Changes

* [5d58f6c](https://github.com/SlideRuleEarth/sliderule/commit/5d58f6cbcd5b7ed7337977ac65f161afb931bf5f) - added static website to release process

* Changed `test_runner.lua` to automatically detect self tests and run them; automated check for running in the cloud

* `nodejs` client reverted to original code base

## Getting This Release

[https://github.com/SlideRuleEarth/sliderule/releases/tag/v4.12.0](https://github.com/SlideRuleEarth/sliderule/releases/tag/v4.12.0)
