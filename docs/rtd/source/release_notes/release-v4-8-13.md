# Release v4.8.13

2024-11-14

Version description of the v4.8.13 release of SlideRule Earth.

Bathy Version #12.

## ATL24 Changes

* Updated qtrees model

* Error handling updated so that if an error is encountered during creation of a dataframe in the C++ code, the entire granule is not generated.  This is different than the way the Python code handles errors.  In the Python code, if an exception is thrown, only that beam is removed from the granule.  The reason for this change is that the C++ code relies on both CMR and S3 which are external services, which upon occasion have failures.  When those failures occur, we don't want to just remove the beam that they occurred on, we want to rerun the entire granule.  But for the Python code, everything is self-contained, and if there is an error, it is an error in the code itself that will occur on every rerun.

## Known Issues and Remaining Tasks

## Getting This Release

[https://github.com/SlideRuleEarth/sliderule/releases/tag/v4.8.13](https://github.com/SlideRuleEarth/sliderule/releases/tag/v4.8.13)
