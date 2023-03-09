# Release v1.4.6

2022-08-01

Version description of the v1.4.6 release of ICESat-2 SlideRule.

## New Features

- Updated Python client to support ICESat-2's proxied APIs

- Implemented Kmeans clustering support for large regions of interest.  See `icesat2.toregion` function and the `n_clusters` parameter.

## Major Issues Resolved

- Fixed bug [#106](https://github.com/ICESat2-SlideRule/sliderule-python/issues/106) in Python client when a node is identified in multiple concurrent threads to be removed, only the first wins and the others just pass.

## Minor Issues Resolved

-  Fixed crash in Voila demo when no photons are returned. [#101](https://github.com/ICESat2-SlideRule/sliderule-python/issues/101);

## Getting This Release

[https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v1.4.6](https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v1.4.6)

[https://github.com/ICESat2-SlideRule/sliderule-icesat2/releases/tag/v1.4.6](https://github.com/ICESat2-SlideRule/sliderule-icesat2/releases/tag/v1.4.6)

[https://github.com/ICESat2-SlideRule/sliderule-python/releases/tag/v1.4.6](https://github.com/ICESat2-SlideRule/sliderule-python/releases/tag/v1.4.6)

