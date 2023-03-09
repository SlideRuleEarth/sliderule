# Release v1.3.0

2021-12-08

Version description of the v1.3.0 release of ICESat-2 SlideRule.

## New Features

* YAPC algorithm implemented as an option when using the atl06, atl06p, atl03s, and atl03sp APIs [#100](https://github.com/ICESat2-SlideRule/sliderule/issues/100)

* Signal confidence selection can be specified as a list in addition to a single value. [#19](https://github.com/ICESat2-SlideRule/sliderule/issues/19)

* There was a memory leak vulnerability in the previous versions of the code which was unlikely but possible.  It could occur when a dataset read timed out.  This vulnerability has been fixed, but the consequence is that the ATL08 processing is significantly slower.  Future versions of the code will attempt to bring the ATL08 processing back down.

## Major Issues Resolved

* Fixed bug in Python client where output was not being sorted correctly by time [ed4999d](https://github.com/ICESat2-SlideRule/sliderule-python/commit/ed4999dda4501cff1772ae7178e84a72e4249fb4)

## Minor Issues Resolved

* Fixed minor issues related to compiling SlideRule with clang [3c80dbd](https://github.com/ICESat2-SlideRule/sliderule-python/commit/3c80dbd5068608094a2b5ad8af0245b2f39e3e87)

## Getting This Release

[https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v1.3.0](https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v1.3.0)

[https://github.com/ICESat2-SlideRule/sliderule-icesat2/releases/tag/v1.3.0](https://github.com/ICESat2-SlideRule/sliderule-icesat2/releases/tag/v1.3.0)

[https://github.com/ICESat2-SlideRule/sliderule-python/releases/tag/v1.3.0](https://github.com/ICESat2-SlideRule/sliderule-python/releases/tag/v1.3.0)

