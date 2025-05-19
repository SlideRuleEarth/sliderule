# Release v4.8.1

2024-10-22

Version description of the v4.8.1 release of SlideRule Earth.

Bathy Version #6.

## ATL24 Changes

* The `use_predictions` option for CoastNet and OpenOceans++ has been removed from the SlideRule code because there was confusion on how this option was used and it no longer needed.  SlideRule now clears the predictions column before sending the photon data to CoastNet and OpenOceans++.

* The default parameters for OpenOceans++ are used as is and the code that supports overriding those parameters has been removed.  This will be restore at some level in the future, but for now, it greatly simplifies updates to the OpenOceans++ glue code.

## Known Issues and Remaining Tasks

* There are classifier mismatches for the ATL24 classifications

## General Changes

* CMR and STAC query support has been restored. A change in the CMR and STAC service removed support for scrolling via paging which is what SlideRule was doing.  The use of `next` and `scroll-after` has now been implemented.

## Getting This Release

[https://github.com/SlideRuleEarth/sliderule/releases/tag/v4.8.1](https://github.com/SlideRuleEarth/sliderule/releases/tag/v4.8.1)
