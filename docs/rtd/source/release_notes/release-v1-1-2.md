# Release v1.1.2

2021-09-03

Version description of the v1.1.2 release of ICESat-2 SlideRule.

## New Features

* The api_widgets_demo updated to provide interface for creating polygon/bounding boxes, and to select a land class.

## Major Issues Resolved

* H5Coro memory usage was optimized to support runs against larger datasets

## Minor Issues Resolved

* GeoDataFrame index (which is a datetime calculated from the delta_time column) is now set to ns resolution so that all rows are unique. [ef2abce](https://github.com/ICESat2-SlideRule/sliderule-python/commit/ef2abce6d406cb1865c78ce6e0380063263c2336)

* Fixed multiple memory leaks in h5coro, in the use of an asset object in reader classes, and in various other modules throughout the server code.

* Fixed startup bug where client that needs credentials would never be able to get those credentials if created before the first set of credentials were available. [da139e6](https://github.com/ICESat2-SlideRule/sliderule/commit/da139e6fc395ed833a7ddf764ea9b3967385f6b3)



## Getting this release

[https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v1.1.2](https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v1.1.2)

[https://github.com/ICESat2-SlideRule/sliderule-icesat2/releases/tag/v1.1.2](https://github.com/ICESat2-SlideRule/sliderule-icesat2/releases/tag/v1.1.2)

[https://github.com/ICESat2-SlideRule/sliderule-python/releases/tag/v1.1.2](https://github.com/ICESat2-SlideRule/sliderule-python/releases/tag/v1.1.2)

