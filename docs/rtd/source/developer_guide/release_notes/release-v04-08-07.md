# Release v4.8.7

2024-10-25

Version description of the v4.8.7 release of SlideRule Earth.

Bathy Version #10.

## ATL24 Changes

* Empty dataframes are not sent to GeoDataFrame runners; this is to protect against classifiers coredumping when a dataframe of zero length is passed to them.

* When a granule produces no beams with any photons within the DEM range, there is an "empty" marker file produced containing the reason for the failure.

* When a granule has a beam that encounters an error in the SlideRule infrastructure code (e.g. writing the arrow file fails), there is an "empty" marker file produced containing the reason for the failure.

* When a classifier in the docker container encounters an error, the exception is recorded and the occurrence of the error is provided in the metadata output.

* Coastnet was updated to handle the non-deterministic behavior of casting a signed double to an unsigned long (arm behaves differently than x86).

## Known Issues and Remaining Tasks

* There are classifier mismatches for the ATL24 classifications; but increasingly they appear to be related to the arm64 vs x86 architecture differences.

* The coastnet and qtrees versions have the "dirty" designation due to the way the docker image is built.  When the docker image is built and the coastnet and qtrees repositories are copied into the docker image, the "models" directories are excluded to keep the size of the docker image down.  But the git describe command is run against the version of the repository inside the docker image, the one without the models directory which is why git describe reports "dirty".

## Getting This Release

[https://github.com/SlideRuleEarth/sliderule/releases/tag/v4.8.7](https://github.com/SlideRuleEarth/sliderule/releases/tag/v4.8.7)
