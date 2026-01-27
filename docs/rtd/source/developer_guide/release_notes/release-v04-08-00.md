# Release v4.8.x

2024-11-16

Version description of the v4.8.15 release of SlideRule Earth.

| Sliderule Version | Bathy Version |
|:-----------------:|:-------------:|
| v4.8.0            | #5            |
| v4.8.1            | #6            |
| v4.8.4            | #7            |
| v4.8.5            | #8            |
| v4.8.6            | #9            |
| v4.8.7            | #10           |
| v4.8.8            | #11           |
| v4.8.11           | #11           |
| v4.8.13           | #12           |
| v4.8.14           | #13           |

## ATL24 Changes

* **v4.8.15** - Added BlueTopo sampling support

* **v4.8.15** - Change availability zone to `d`

* **v4.8.15** - Changed cache duration of the pre-flight cors request from 1 year to 1 hour

* **v4.8.15** - ATL24 - YAPC in processing flags

* **v4.8.15** - Fixed bug in `atl08`, `x_atc` type

* **v4.8.15** - Fixed bug in `atl06`, uninitialized memory used in `w_surface_window_final`

* **Bathy Version #11** - Updated ensemble model

* **Bathy Version #11** - track_stacker code (ensemble) now called directly

* **Bathy Version #11** - track_stacker includes some blunder detection

* **Bathy Version #11** - ensemble confidence included in output

* **Bathy Version #11** - additional version information (track_stacker git commit id and ensemble model name) included in metadata

* **Bathy Version #11** - Updated qtrees model

* **Bathy Version #11** - Error handling updated so that if an error is encountered during creation of a dataframe in the C++ code, the entire granule is not generated.  This is different than the way the Python code handles errors.  In the Python code, if an exception is thrown, only that beam is removed from the granule.  The reason for this change is that the C++ code relies on both CMR and S3 which are external services, which upon occasion have failures.  When those failures occur, we don't want to just remove the beam that they occurred on, we want to rerun the entire granule.  But for the Python code, everything is self-contained, and if there is an error, it is an error in the code itself that will occur on every rerun.

* **Bathy Version #11** - Updated coastnet code and model

* **Bathy Version #11** - Added geoid filter

* **Bathy Version #11** - iso xml filename no longer includes ".h5" in the name

* **Bathy Version #11** - the h5 granule name has the release and version appended to it (e.g. *001_01.h5)

* **Bathy Version #11** - Updated ensemble model

* **Bathy Version #10** - Empty dataframes are not sent to GeoDataFrame runners; this is to protect against classifiers coredumping when a dataframe of zero length is passed to them.

* **Bathy Version #10** - When a granule produces no beams with any photons within the DEM range, there is an "empty" marker file produced containing the reason for the failure.

* **Bathy Version #10** - When a granule has a beam that encounters an error in the SlideRule infrastructure code (e.g. writing the arrow file fails), there is an "empty" marker file produced containing the reason for the failure.

* **Bathy Version #10** - When a classifier in the docker container encounters an error, the exception is recorded and the occurrence of the error is provided in the metadata output.

* **Bathy Version #10** - Coastnet was updated to handle the non-deterministic behavior of casting a signed double to an unsigned long (arm behaves differently than x86).

* **Bathy Version #09** - The ensemble model was updated.

* **Bathy Version #08** - The CoastNet and Qtrees models were updated.

* **Bathy Version #08** - OpenOceans++ was updated.

* **Bathy Version #08** - Error handling was added to OceanEyes python runner so that if any of the classifiers throws an exception, the beam the exception was thrown on is removed from the final output.

* **Bathy Version #08** - A wind speed of 3.3m is used for any granules where an ATL09 granule is not present.  (The previous value was 0.0).

* **Bathy Version #07** - The CoastNet and Qtrees models were updated.

* **Bathy Version #06** - The `use_predictions` option for CoastNet and OpenOceans++ has been removed from the SlideRule code because there was confusion on how this option was used and it no longer needed.  SlideRule now clears the predictions column before sending the photon data to CoastNet and OpenOceans++.

* **Bathy Version #06** - The default parameters for OpenOceans++ are used as is and the code that supports overriding those parameters has been removed.  This will be restore at some level in the future, but for now, it greatly simplifies updates to the OpenOceans++ glue code.

* **Bathy Version #05** - Fixed issue with ortho_h where it was being set to zero when the surface_h was less than or equal to zero.

* **Bathy Version #05** - When Kd is invalid (i.e. No Data from the VIIRS raster), the previous Kd is used and an INVALID_KD flag is set

* **Bathy Version #05** - Fixed issue where the absolute value of the pointing angle is now being used to calculate the pointing angle index

* **Bathy Version #05** - Fixed issue with the uncertainty calculation where the uncertainties are now the RSS of all the contributing factors

* **Bathy Version #05** - Fixed issue with the use of the wind speed in the uncertainty calculation where wind speeds are now averaged between five different levels

* **Bathy Version #05** - Coastnet is used to set the surface_h

* **Bathy Version #05** - Minimum subaqueous vertical uncertainty set to 10cm

* **Bathy Version #05** - Refraction correction and uncertainty applied to photons below surface_h AND are not labeled as sea surface

* **Bathy Version #05** - Classifiers updated: coastnet, qtrees, openoceanspp, ensemble

* **Bathy Version #05** - Statistics on Subaqueous photons no longer include photons labeled as sea surface

## Known Issues and Remaining Tasks

* **v4.8.0** - CMR queries of ATL09 stopped working - CMR updated their API and removed support for page numbers when scrolling

## General Changes

* **v4.8.1** - CMR and STAC query support has been restored. A change in the CMR and STAC service removed support for scrolling via paging which is what SlideRule was doing.  The use of `next` and `scroll-after` has now been implemented.

* **v4.8.0** - GEDI parameters updated to be more accurate an intuitive (the old parameter names are still functional but have been DEPRECATED, please switch to using the new ones in your code).
    - `degrade_flag` changed to `degrade_filter` and instead of having to know what the flag values were; the user now just needs to set to True to filter out all degraded footprints.
    - `l2_quality_flag` changed to `l2_quality_filter` and instead of having to know what the flag values were; the user now just needs to set to True to filter out anything that does not meet the L2 quality criteria.
    - `l4_quality_flag` changed to `l4_quality_filter` and instead of having to know what the flag values were; the user now just needs to set to True to filter out anything that does not meet the L4 quality criteria.
    - `surface_flag` changed to `surface_filter` and instead of having to know what the flag values were; the user now just needs to set to True to filter out anything that isn't a surface footprint.
    - `beam` changed to `beams` to reflect that it is primarily a list of beams to process and not just a single beam (even though if the user only provides a single beam, the server side code will automatically promote it to a list of beams with only one element).

* **v4.8.0** - Subsetting requests that use a rasterized geojson to perform the subsetting have been changed so that the geojson string is passed via a parameter named `region_mask` instead of `raster`.  The original name was confusing with our increasing support for raster sampling.  The new name makes it clearer that the parameter is used as a mask for the region of interest.

## Known Issues and Remaining Tasks

* **Bathy Version #10** - There are classifier mismatches for the ATL24 classifications; but increasingly they appear to be related to the arm64 vs x86 architecture differences.

* **Bathy Version #10** - The coastnet and qtrees versions have the "dirty" designation due to the way the docker image is built.  When the docker image is built and the coastnet and qtrees repositories are copied into the docker image, the "models" directories are excluded to keep the size of the docker image down.  But the git describe command is run against the version of the repository inside the docker image, the one without the models directory which is why git describe reports "dirty".

* **Bathy Version #09** - There are classifier mismatches for the ATL24 classifications; but increasingly they appear to be related to the arm64 vs x86 architecture differences.

* **Bathy Version #09** - The coastnet and qtrees versions have the "dirty" designation due to the way the docker image is built.  When the docker image is built and the coastnet and qtrees repositories are copied into the docker image, the "models" directories are excluded to keep the size of the docker image down.  But the git describe command is run against the version of the repository inside the docker image, the one without the models directory which is why git describe reports "dirty".

* **Bathy Version #08** - There are classifier mismatches for the ATL24 classifications; but increasingly they appear to be related to the arm64 vs x86 architecture differences.

* **Bathy Version #07** - There are classifier mismatches for the ATL24 classifications

## Getting This Release

[https://github.com/SlideRuleEarth/sliderule/releases/tag/v4.8.15](https://github.com/SlideRuleEarth/sliderule/releases/tag/v4.8.15)
