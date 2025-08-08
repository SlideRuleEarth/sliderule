# Release v4.8.5

2024-10-23

Version description of the v4.8.5 release of SlideRule Earth.

Bathy Version #8.

## ATL24 Changes

* The CoastNet and Qtrees models were updated.

* OpenOceans++ was updated.

* Error handling was added to OceanEyes python runner so that if any of the classifiers throws an exception, the beam the exception was thrown on is removed from the final output.

* A wind speed of 3.3m is used for any granules where an ATL09 granule is not present.  (The previous value was 0.0).

## Known Issues and Remaining Tasks

* There are classifier mismatches for the ATL24 classifications; but increasingly they appear to be related to the arm64 vs x86 architecture differences.

## Getting This Release

[https://github.com/SlideRuleEarth/sliderule/releases/tag/v4.8.5](https://github.com/SlideRuleEarth/sliderule/releases/tag/v4.8.5)
