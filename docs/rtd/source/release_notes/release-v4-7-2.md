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

## General Changes

* GEDI parameters updated to be more accurate an intuitive.
    - `degrade_flag` changed to `degrade_filter` and instead of having to know what the flag values were; the user now just needs to set to True to filter out all degraded footprints.
    - `l2_quality_flag` changed to `l2_quality_filter` and instead of having to know what the flag values were; the user now just needs to set to True to filter out anything that does not meet the L2 quality criteria.
    - `l4_quality_flag` changed to `l4_quality_filter` and instead of having to know what the flag values were; the user now just needs to set to True to filter out anything that does not meet the L4 quality criteria.
    - `surface_flag` changed to `surface_filter` and instead of having to know what the flag values were; the user now just needs to set to True to filter out anything that isn't a surface footprint.
    - `beam` changed to `beams` to reflect that it is primarily a list of beams to process and not just a single beam (even though if the user only provides a single beam, the server side code will automatically promote it to a list of beams with only one element).

## Development Updates

## Getting This Release

[https://github.com/SlideRuleEarth/sliderule/releases/tag/v4.7.2](https://github.com/SlideRuleEarth/sliderule/releases/tag/v4.7.2)

## ATL24 Request Parameters

## Benchmarks
