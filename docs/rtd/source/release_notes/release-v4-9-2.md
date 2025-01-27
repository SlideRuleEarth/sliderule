# Release v4.9.2

2025-01-27

Version description of the v4.9.2 release of SlideRule Earth.

Bathy Version #14.

## SlideRule Changes

* Optimized raster sampling code
* Fixed Python client to support output format specified as geoparquet with open_on_complete
* Changed default atl03 confidence flags to low, medium, and high
* Added separate geophysical corrections ancillary fields list in support of future ATL03 dataframe class

## ATL24 Specific Changes

* Separated out processing flags into their own variables in the h5 file: sensor depth exceeded, invalid kd, invalid wind speed, night flight
* Added low confidence flag to h5
* Added ensemble confidence to h5
* ISO.XML polygon is now taken directly from ATL03

## Known Issues and Remaining Tasks

## Getting This Release

[https://github.com/SlideRuleEarth/sliderule/releases/tag/v4.9.2](https://github.com/SlideRuleEarth/sliderule/releases/tag/v4.9.2)
