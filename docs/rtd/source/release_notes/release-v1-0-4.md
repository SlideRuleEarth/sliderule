# Release v1.0.4

2021-05-07

Version description of the v1.0.4 release of ICESat-2 SlideRule

* The ATL06 response records now include the `h_sigma` field ([833a3bd](https://github.com/ICESat2-SlideRule/sliderule-icesat2/commit/833a3bd7feca8deb77b7671ef96b0adfda07a48c))

* Updated H5Coro to support version 4 ATL03 files ([6198cec](https://github.com/ICESat2-SlideRule/sliderule/commit/6198cec550268ca66784dd2c3b0be1a53f94d6bf))

* Fixed symbol table bug in H5Coro ([f80857c](https://github.com/ICESat2-SlideRule/sliderule/commit/f80857ce83d73d6ec3ba71083de6981f3570c95a))

* Resource names are sanitized before being used ([61f768c](https://github.com/ICESat2-SlideRule/sliderule/commit/61f768c825bc41074fdc20ff1269128f29bffb47))

* Renamed ***h5coro-python*** target to ***binding-python***, and it is now treated as Python bindings for SlideRule.  Python bindings are not intended to become a Python package.

* Created Python utility for displaying version of SlideRule running ([query_version.py](https://github.com/ICESat2-SlideRule/sliderule-python/blob/main/utils/query_version.py))

* `icesat2.toregion` api now supports GeoJSON, Shapefiles, and simplifying polygons (see [documentation](/rtd/user_guide/ICESat-2.html#toregion))

* Created `sliderule-project` repository for icesat2sliderule.org; all documentation moved to this repository (includes Read the Docs).

* Optimized `icesat2.h5` api by using _bytearray_ when concatenating responses ([da05da6](https://github.com/ICESat2-SlideRule/sliderule-python/commit/da05da6a272e7decc75b75bf11716be383cb53d7))


## Getting this release

[https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v1.0.4](https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v1.0.4)


