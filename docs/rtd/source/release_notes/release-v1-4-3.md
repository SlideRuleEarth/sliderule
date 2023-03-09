# Release v1.4.3

2022-05-31

Version description of the v1.4.3 release of ICESat-2 SlideRule.

## New Features

## Major Issues Resolved

- Updated YAPC algorithm to Jeff's latest specification (05-23-22): version 3.0.  The new algorithm is the default version that runs.  If the previous algorithm is desired, there is a `version` parameter which is a part of the `yapc` parameter block that can be set to "1", and the original algorithm will execute.  Note: the new algorithm runs about three times faster than the original one.

- Fixed calculation of the segment id.  There was a rounding error on the server side and a parsing error on the construction of the dataframe in the client. [a5db62a](https://github.com/ICESat2-SlideRule/sliderule-python/commit/a5db62ad9b7570e25ab7105eaec06267e4fadf11), and [ec2b1f6](https://github.com/ICESat2-SlideRule/sliderule-icesat2/commit/ec2b1f6bc53c4f1e93f0dcffa7abd4dcec8379c6).

## Minor Issues Resolved

## Getting This Release

[https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v1.4.3](https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v1.4.3)

[https://github.com/ICESat2-SlideRule/sliderule-icesat2/releases/tag/v1.4.3](https://github.com/ICESat2-SlideRule/sliderule-icesat2/releases/tag/v1.4.3)

[https://github.com/ICESat2-SlideRule/sliderule-python/releases/tag/v1.4.3](https://github.com/ICESat2-SlideRule/sliderule-python/releases/tag/v1.4.3)

