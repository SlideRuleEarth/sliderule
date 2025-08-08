# Release v4.8.0

2024-10-15

Version description of the v4.8.0 release of SlideRule Earth.

Bathy Version #5.

## ATL24 Changes

* Fixed issue with ortho_h where it was being set to zero when the surface_h was less than or equal to zero.

* When Kd is invalid (i.e. No Data from the VIIRS raster), the previous Kd is used and an INVALID_KD flag is set

* Fixed issue where the absolute value of the pointing angle is now being used to calculate the pointing angle index

* Fixed issue with the uncertainty calculation where the uncertainties are now the RSS of all the contributing factors

* Fixed issue with the use of the wind speed in the uncertainty calculation where wind speeds are now averaged between five different levels

* Coastnet is used to set the surface_h

* Minimum subaqueous vertical uncertainty set to 10cm

* Refraction correction and uncertainty applied to photons below surface_h AND are not labeled as sea surface

* Classifiers updated: coastnet, qtrees, openoceanspp, ensemble

* Statistics on Subaqueous photons no longer include photons labeled as sea surface

## Known Issues and Remaining Tasks

* CMR queries of ATL09 stopped working - CMR updated their API and removed support for page numbers when scrolling

## General Changes

* GEDI parameters updated to be more accurate an intuitive (the old parameter names are still functional but have been DEPRECATED, please switch to using the new ones in your code).
    - `degrade_flag` changed to `degrade_filter` and instead of having to know what the flag values were; the user now just needs to set to True to filter out all degraded footprints.
    - `l2_quality_flag` changed to `l2_quality_filter` and instead of having to know what the flag values were; the user now just needs to set to True to filter out anything that does not meet the L2 quality criteria.
    - `l4_quality_flag` changed to `l4_quality_filter` and instead of having to know what the flag values were; the user now just needs to set to True to filter out anything that does not meet the L4 quality criteria.
    - `surface_flag` changed to `surface_filter` and instead of having to know what the flag values were; the user now just needs to set to True to filter out anything that isn't a surface footprint.
    - `beam` changed to `beams` to reflect that it is primarily a list of beams to process and not just a single beam (even though if the user only provides a single beam, the server side code will automatically promote it to a list of beams with only one element).

* Subsetting requests that use a rasterized geojson to perform the subsetting have been changed so that the geojson string is passed via a parameter named `region_mask` instead of `raster`.  The original name was confusing with our increasing support for raster sampling.  The new name makes it clearer that the parameter is used as a mask for the region of interest.

## Getting This Release

[https://github.com/SlideRuleEarth/sliderule/releases/tag/v4.8.0](https://github.com/SlideRuleEarth/sliderule/releases/tag/v4.8.0)
