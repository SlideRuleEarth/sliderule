# Release v1.4.1

2022-04-25

Version description of the v1.4.1 release of ICESat-2 SlideRule.

> :warning: This is the last release of the client to default to version '004' of the ICESat-2 ATL03 dataset.  The next release will default to using version '005' of the ICESat-2 ATL03 dataset.

## New Features

- Updated notes and documentation inside Python demos

- Added photon classification widgets

- Updates to `io.py` module [sliderule-python#91](https://github.com/ICESat2-SlideRule/sliderule-python/pull/91), [sliderule-python#93](https://github.com/ICESat2-SlideRule/sliderule-python/pull/93)

- Added CredentialStore bindings to Python target: [pyCredentialStore](https://github.com/ICESat2-SlideRule/sliderule/blob/main/targets/binding-python/pyCredentialStore.cpp)

- Set log retention policies in Loki, with rolling backups to S3

- Added disk metrics for the Monitor EC2 instance to grafana

- Country of origin statistics [sliderule-project#15](https://github.com/ICESat2-SlideRule/sliderule-project/issues/15)

- Added `spot` column to the ***atl03s*** and ***atl03sp*** returned geodataframes. [177792f](https://github.com/ICESat2-SlideRule/sliderule-python/commit/177792fac19a3ef0ba10a73d09b6fe7f1a70c48c)

## Major Issues Resolved

- Fixed multiple bugs leading to missing elevations on some processing runs. [sliderule#92](https://github.com/ICESat2-SlideRule/sliderule/issues/92).  A result of fixing this bug is that users now see messages like "RESOURCE NOT FOUND" when requesting granules that don't exist. [sliderule#41](https://github.com/ICESat2-SlideRule/sliderule/issues/41).

- Fixed bug in H5Coro when processing chunked datasets when the chunk dimensions do not span the full number of columns. [c48b918](https://github.com/ICESat2-SlideRule/sliderule/commit/c48b918e12080fb97fb4bb6e3849841eadc75ed2)

- SlideRule server code now returns an ***Exception Record***(`exceptrec`) back to the client when a processing exception occurs.  The client can use this to retry the request or provide diagnostic information to the user as to what went wrong.  The SlideRule Python client will retry the request depending on the severity of the failure. [a1a9b7b](https://github.com/ICESat2-SlideRule/sliderule/commit/a1a9b7b8e7a4c29e834631f50f06ddb5fa612193)

## Minor Issues Resolved

- Voila demo updates:
    - Increased maximum number of resources that can be processed in a request to 1,000
    - Handle case where no results are returned
    - Handle case where user does not draw a polygon before making a request
    - Use updated ipysliderule widgets for RGT, GT, and cycle
    - Fixed progress message to accurately report number of returned elevations per resource

- Updated pistache to latest release [sliderule#86](https://github.com/ICESat2-SlideRule/sliderule/issues/86)

- The day and the month is reported in log messages instead of the day of year.  This was implemented in a previous release, but was missed and is now being included in these release notes for completeness.  [sliderule#69](https://github.com/ICESat2-SlideRule/sliderule/issues/69)

## Getting This Release

[https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v1.4.1](https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v1.4.1)

[https://github.com/ICESat2-SlideRule/sliderule-icesat2/releases/tag/v1.4.1](https://github.com/ICESat2-SlideRule/sliderule-icesat2/releases/tag/v1.4.1)

[https://github.com/ICESat2-SlideRule/sliderule-python/releases/tag/v1.4.1](https://github.com/ICESat2-SlideRule/sliderule-python/releases/tag/v1.4.1)

