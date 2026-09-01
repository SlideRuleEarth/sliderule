# Release v4.9.x

2025-02-04

Version description of the v4.9.3 release of SlideRule Earth.

| Sliderule Version | Bathy Version |
|:-----------------:|:-------------:|
| v4.9.2            | #14           |
| v4.9.3            | #15           |

## Changes

* **v4.9.2** - Optimized raster sampling code
* **v4.9.2** - Fixed Python client to support output format specified as geoparquet with open_on_complete
* **v4.9.2** - Changed default atl03 confidence flags to low, medium, and high
* **v4.9.2** - Added separate geophysical corrections ancillary fields list in support of future ATL03 dataframe class
* **v4.9.0** - Added ancillary field support to GEDI (`gedi01bp`, `gedi02ap`, `gedi04ap`)
* **Bathy Version #15** - Separated out processing flags into their own variables in the h5 file: sensor depth exceeded, invalid kd, invalid wind speed, night flight
* **Bathy Version #15** - Added low confidence flag to h5
* **Bathy Version #15** - Added ensemble confidence to h5
* **Bathy Version #15** - ISO.XML polygon is now taken directly from ATL03
* **Bathy Version #14** - Updated ensemble

## Getting This Release

[https://github.com/SlideRuleEarth/sliderule/releases/tag/v4.9.3](https://github.com/SlideRuleEarth/sliderule/releases/tag/v4.9.3)
