# Release v4.2.0

2024-01-18

Version description of the v4.2.0 release of ICESat-2 SlideRule.

## Issues Resolved

* [156](https://github.com/ICESat2-SlideRule/sliderule/issues/156), [117](https://github.com/ICESat2-SlideRule/sliderule/issues/117) - large processing runs on full granules now lock the entire node they are running on

* [df1f83e](https://github.com/ICESat2-SlideRule/sliderule/commit/df1f83e6c8572f9deffb5d48ac3bde5f41a7c1fc) - logging in Python client behaves more consistently and is more intuitive

* [0f8039d](https://github.com/ICESat2-SlideRule/sliderule/commit/0f8039dfdc3c97f5a8a6183660fdc9a89925a1ae) - the server code now uses Instance Metadata Service Version 2 (IMDSv2) for accessing metadata associated with the EC2 it is running on.  This is required for some instance types and allows SlideRule to be run on GPU enhanced instances.

## Development Updates

* Node.js client now supports a basic ATL06 API call.

* Amazon Linux 2023 is the new baseline image for the EC2 instances as well as the SlideRule docker container.  This provides up-to-date tooling for building the code, and the latest packages for running in the AWS environment.

* [287](https://github.com/ICESat2-SlideRule/sliderule/issues/287) - dependencies in the build environment are captured in a lock file

## Getting This Release

[https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v4.2.0](https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v4.2.0)

## Benchmarks

```
```