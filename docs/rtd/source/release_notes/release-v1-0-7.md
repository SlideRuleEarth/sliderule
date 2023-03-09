# Release v1.0.7

2021-06-29

Version description of the v1.0.7 release of ICESat-2 SlideRule

* Resolved background density calculation issue: [sliderule#2](https://github.com/ICESat2-SlideRule/sliderule/issues/2)

* Resolved background rate interpolation issue: [sliderule#3](https://github.com/ICESat2-SlideRule/sliderule/issues/3)

* Resolved spacecraft velocity calculation issue: [sliderule#5](https://github.com/ICESat2-SlideRule/sliderule/issues/5)

* Resolved along-track distance calculation issue: [sliderule#73](https://github.com/ICESat2-SlideRule/sliderule/issues/73)

* ATL06-SR segment IDs are calculated by finding the center of the segment and using the segment ID of the closest ATL06 segment: [sliderule#72](https://github.com/ICESat2-SlideRule/sliderule/issues/72)

* Resolved consul exit functionality regression: devops#42

* Added health checking for SlideRule server nodes, with automatic restarts: devops#43
  * rate of once per minute
  * 3 retries: 5 seconds, 10 seconds, 30 seconds
  * unhealthy: connection failure, or health endpoint returns false

## Getting this release

[https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v1.0.7](https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v1.0.7)

[https://github.com/ICESat2-SlideRule/sliderule-icesat2/releases/tag/v1.0.7](https://github.com/ICESat2-SlideRule/sliderule-icesat2/releases/tag/v1.0.7)

[https://github.com/ICESat2-SlideRule/sliderule-python/releases/tag/v1.0.7](https://github.com/ICESat2-SlideRule/sliderule-python/releases/tag/v1.0.7)

