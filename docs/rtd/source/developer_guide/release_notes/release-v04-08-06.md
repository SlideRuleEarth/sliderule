# Release v4.8.6

2024-10-25

Version description of the v4.8.6 release of SlideRule Earth.

Bathy Version #9.

## ATL24 Changes

* The ensemble model was updated.

## Known Issues and Remaining Tasks

* There are classifier mismatches for the ATL24 classifications; but increasingly they appear to be related to the arm64 vs x86 architecture differences.

* The coastnet and qtrees versions have the "dirty" designation due to the way the docker image is built.  When the docker image is built and the coastnet and qtrees repositories are copied into the docker image, the "models" directories are excluded to keep the size of the docker image down.  But the git describe command is run against the version of the repository inside the docker image, the one without the models directory which is why git describe reports "dirty".

## Getting This Release

[https://github.com/SlideRuleEarth/sliderule/releases/tag/v4.8.6](https://github.com/SlideRuleEarth/sliderule/releases/tag/v4.8.6)
