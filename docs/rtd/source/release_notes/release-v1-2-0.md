# Release v1.2.0

2021-11-17

Version description of the v1.2.0 release of ICESat-2 SlideRule.

***Important***: Consul service discovery replaced by internal solution which is now accessed via a different port (8050 instead of 8500).  This breaks backward compatibility of the Python client, so the client needs to be updated to use this latest release.

## New Features

* The along track distance from the equator of each elevation and each photon has been added to their respective record definitions.  For the **atl03** API's the field name is `segment_dist`; for the **atl06** API's the field name is `distance`.

* The **atl03** and **atl06** API's now support specifying the `len` (extent length) and `res` (extent resolution) parameters in terms of whole ATL03 segments.  This is achieved by setting the `dist_in_seg` field in the request parameters to true.

* Added `post` function to the **netsvc** package Lua function calls.

## Major Issues Resolved

* H5Coro supports reading metadata of dataset without reading the data [#96](https://github.com/ICESat2-SlideRule/sliderule/issues/96)

* H5Coro supports reading attributes [#97](https://github.com/ICESat2-SlideRule/sliderule/issues/97)

## Minor Issues Resolved

* Python config makefile target now works without first a full standard install of sliderule [#81](https://github.com/ICESat2-SlideRule/sliderule/issues/81)

* Runtime exceptions caught by PyBind11 when using parallel H5Coro API [#82](https://github.com/ICESat2-SlideRule/sliderule/issues/82)

* Fixed bug in H5Coro processing of BTrees when symbols spanned multiple leaf nodes [4805819](https://github.com/ICESat2-SlideRule/sliderule/commit/4805819dac3029c4767f9957271f73548c46748f)

* Fixed memory leak in PyBind11 wrapper for H5Coro [1206fc5](https://github.com/ICESat2-SlideRule/sliderule/commit/1206fc5864eb965dec2aa34f10116cc74ba7f2b7)

## Getting This Release

[https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v1.2.0](https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v1.2.0)

[https://github.com/ICESat2-SlideRule/sliderule-icesat2/releases/tag/v1.2.0](https://github.com/ICESat2-SlideRule/sliderule-icesat2/releases/tag/v1.2.0)

[https://github.com/ICESat2-SlideRule/sliderule-python/releases/tag/v1.2.0](https://github.com/ICESat2-SlideRule/sliderule-python/releases/tag/v1.2.0)

