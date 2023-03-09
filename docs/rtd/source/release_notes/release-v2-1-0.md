# Release v2.1.0

2023-03-03

Version description of the v2.1.0 release of ICESat-2 SlideRule.

## Major Changes

- The biggest user-facing change is the reorganization of the Python client modules.  With new missions being supported, code residing in the `icesat2` module that is common to multiple missions has been moved to other modules.  The original functions have remained and are implemented as wrappers for the new functions, accompanied by a deprecation warning.  The following functions have been moved:
  * `sliderule.init` provides a generic version of `icesat2.init`
  * `icesat2.toregion` is now `sliderule.toregion`
  * `icesat2.get_version` is now `sliderule.get_version`
  * `icesat2.cmr` is now `earthdata.cmr`
  * `icesat2.set_max_resources` is now `earthdata.set_max_resources`
  * `icesat2.h5` and `icesat2.h5p` are now `h5.h5` and `h5.h5p` respectively

- Additional fields are returned in the ATL03 extents (`atl03s` and `atl03sp`).  Background counts (`background_rate`) and solar elevation (`solar_elevation`) is now returned with all ATL03 subsetting requests.  When PhoREAL processing is enabled (via `phoreal` parameter), then `snowcover` and `landcover` flags are also returned with ATL03 subsetting requests.

## New Features

- GeoParquet output option fully supported, including user specified S3 bucket as a destination; [#72](https://github.com/ICESat2-SlideRule/sliderule-python/issues/72) [#171](https://github.com/ICESat2-SlideRule/sliderule/issues/171)

- Full raster sampling support for ArcticDEM Mosaic and Strips, and REMA; this includes Python client side updates needed to efficiently represent the returned sample data in the GeoDataFrames; [#165](https://github.com/ICESat2-SlideRule/sliderule/issues/165)

- Raster sampling now supports time range filters `t0` and `t1`, clostest time filters `closest_time`, and substring filters `substr`

- Raster sampling now supports including additional flags with each sample via the `with_flags` option

- Streamlined private cluster setup in the Python client; added `sliderule.scaleout` and udpated behavior of `sliderule.init` function to transparently handle starting and scaling a cluster if desired; [#126](https://github.com/ICESat2-SlideRule/sliderule-python/issues/126)

- Python client supports TIME8 fields from SlideRule

- PhoREAL/atl08 endpoint added as a feature-preview

- GEDI L4A subsetting endpoint added as a feature-preview

## Development Updates

- GitHub actions added to add all new issues to the SlideRule project; [#98](https://github.com/ICESat2-SlideRule/sliderule/issues/98)

- HTTPS termination handled by HAProxy which provides faster startup times; [#45](https://github.com/ICESat2-SlideRule/sliderule-build-and-deploy/issues/45)

- RecordObject posting code consolidated [#164](https://github.com/ICESat2-SlideRule/sliderule/issues/164)

- The Python 3.8 pin was removed from the environment.yml file; [#127](https://github.com/ICESat2-SlideRule/sliderule-python/issues/127)

- Reworked parameter handling on the server side and created/updated the Icesat2Parms, ArrowParms, and GeoParms classes

## Issues Resolved

- Resolved deprecation warning from Shapely in Python client; [#95](https://github.com/ICESat2-SlideRule/sliderule-python/issues/95)

- H5Coro bug fixes and support for the attribute info message; [#177](https://github.com/ICESat2-SlideRule/sliderule/pull/177)

## Known Issues

- PhoREAL processing includes some known bugs - the median ground height uses the relative heights instead of absolute heights, and the canopy openess calculation is incorrect

## Getting This Release

[https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v2.1.0](https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v2.1.0)

[https://github.com/ICESat2-SlideRule/sliderule-icesat2/releases/tag/v2.1.0](https://github.com/ICESat2-SlideRule/sliderule-icesat2/releases/tag/v2.1.0)

[https://github.com/ICESat2-SlideRule/sliderule-python/releases/tag/v2.1.0](https://github.com/ICESat2-SlideRule/sliderule-python/releases/tag/v2.1.0)

