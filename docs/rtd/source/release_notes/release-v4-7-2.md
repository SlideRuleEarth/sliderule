# Release v4.7.2

2024-xx-xx

Version description of the v4.7.2 release of SlideRule Earth.

## ATL24 Changes

* Fixed issue with ortho_h where it was being set to zero when the surface_h was less than or equal to zero.

* When Kd is invalid (i.e. No Data from the VIIRS raster), the previous Kd is used and an INVALID_KD flag is set

* Fixed issue where the absolute value of the pointing angle is now being used to calculate the pointing angle index

* Fixed issue with the uncertainty calculation where the uncertainties are now the RSS of all the contributing factors

* Fixed issue with the use of the wind speed in the uncertainty calculation where wind speeds are now averaged between five different levels

## Known Issues and Remaining Tasks

## Development Updates

## Getting This Release

[https://github.com/SlideRuleEarth/sliderule/releases/tag/v4.7.2](https://github.com/SlideRuleEarth/sliderule/releases/tag/v4.7.2)

## ATL24 Request Parameters

## Benchmarks
