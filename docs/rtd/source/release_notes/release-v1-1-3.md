# Release v1.1.3

2021-09-17

Version description of the v1.1.3 release of ICESat-2 SlideRule.

## New Features

* In the `atl03rec` record (icesat2.atl03s and icesat2.atl03sp APIs), the `info` field has been replaced by the `atl08_class` and `atl03_cnf` fields which hold the ATL08 photon classification, and ATL03 photon confidence level respectively.

* The default confidence level for all APIs that accept a confidence level has been changed to background (0) from high (4).

## Major Issues Resolved

None

## Minor Issues Resolved

* An out-of-bounds vulnerability was fixed in the processing of ATL08 photon classifications. [658409c](https://github.com/ICESat2-SlideRule/sliderule-icesat2/commit/658409cb1fac8608b0489e40cfd772cae2b66a01)


## Getting this release

[https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v1.1.3](https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v1.1.3)

[https://github.com/ICESat2-SlideRule/sliderule-icesat2/releases/tag/v1.1.3](https://github.com/ICESat2-SlideRule/sliderule-icesat2/releases/tag/v1.1.3)

[https://github.com/ICESat2-SlideRule/sliderule-python/releases/tag/v1.1.3](https://github.com/ICESat2-SlideRule/sliderule-python/releases/tag/v1.1.3)

