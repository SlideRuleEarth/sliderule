# Release v1.1.0

2021-08-16

Version description of the v1.1.0 release of ICESat-2 SlideRule.

## New Features

* ATL08 classifications are now supported in the `atl06`, `atl06p`, `atl03s`, `atl03sp` APIs: [sliderule#71](https://github.com/ICESat2-SlideRule/sliderule/issues/71)
  * when the request parameters supply a list of ATL08 classifications to use, the server code will read the corresponding ATL08 data and only use the supplied classifications in the calculation
  * the following classifications are supported: noise, ground, canopy, top of canopy, unclassified
  * for the `atl03s`, `atl03sp`, the presence of the ATL08 classification list also enables the returned photon data to include each photons classification

* The following APIs now return GeoDataFrames instead of dictionaries: `atl06`, `atl06p`, `atl03s`, `atl03sp`.
  * this standardizes the return structure at no cost to performance
  * each GeoDataFrame has a _"time"_ column which is a Python `datetime` value
  * each GeoDataFrame uses the ***geometry.x*** and ***geometry.y*** to represent the _"longitude"_ and _"latitude"_ fields respectively.
  * the *"delta_time"* column now represents the time from the ATLAS Standard Data Product (SDP) epoch (January 1, 2018)
  * The GeoDataFrames returned by `atl03s` and `atl03sp` contain a row for each photon that is returned

* All APIs default to using version 004 of the data products.

* Added the ground track field (_"gt"_) to the `atl06` and `atl06p` elevation returns.
  * added the following constants to the **icesat2.py** module: GT1L, GT1R, GT2L, GT2R, GT3L, GT3R
  * you can now do things like:
    ```python
    new_gdf = gdf[gdf["gt"] == icesat2.GT1L]
    ```

* Added STRONG_SPOTS and WEAK_SPOTS constants to the **icesat2.py** module
  * you can no do things like:
    ```python
    new_gdf = atl06[atl06["spot"].isin(icesat2.WEAK_SPOTS)]
    ```

* Python client published to conda-forge: [sliderule](https://anaconda.org/conda-forge/sliderule)

## Major Issues Resolved

* Resolved delta_time calculation issue: [sliderule#48](https://github.com/ICESat2-SlideRule/sliderule/issues/48)

* Resolved latitude and longitude calculation issue: [sliderule#7](https://github.com/ICESat2-SlideRule/sliderule/issues/7)

* HttpServer class fatal exception bug that causes server to hang was fixed: [sliderule#77](https://github.com/ICESat2-SlideRule/sliderule/issues/77)

* Health check made more robust to handle case when server connection hangs: [devops#45](https://github.com/ICESat2-SlideRule/devops/issues/45)

## Minor Issues Resolved

* TCP/IP and HTTP errors are caught and user-friendly error messages are printed: [sliderule#26](https://github.com/ICESat2-SlideRule/sliderule/issues/26)

* Memory leak in Asset class fixed: [sliderule#32](https://github.com/ICESat2-SlideRule/sliderule/issues/32)

* Server generated log messages logged by the Python client are logged at the level specified in the server log message: [sliderule-python#1](https://github.com/ICESat2-SlideRule/sliderule-python/issues/1)

* The Python client logging is now completely turned off if the verbose setting is set to False: [sliderule-python#26](https://github.com/ICESat2-SlideRule/sliderule-python/issues/26)

* Fixed bug in code where the along track spread and the number of photon checks were not being made in the correct place in the code.  In certain circumstances the final elevation could have been calculated on the set of photons that failed validation instead of the preceding set that did not fail the validation.  [cb5948c](https://github.com/ICESat2-SlideRule/sliderule-icesat2/commit/cb5948c8587f841ad6ade758bfeacc4f698786cc)

* CMake build fails configuration if dependency not found: [sliderule#40](https://github.com/ICESat2-SlideRule/sliderule/issues/40)

* Signals issued while mutex held in MsgQ class code: [sliderule#79](https://github.com/ICESat2-SlideRule/sliderule/issues/79)

* Fixed race conditions in SlideRule's Python bindings; specifically fixed the H5Coro API.

* Added `set_rqst_timeout` API to ***icesat2.py*** module to allow user to control what the timeouts are for connecting to and reading from SlideRule servers.

## Getting this release

[https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v1.1.0](https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v1.1.0)

[https://github.com/ICESat2-SlideRule/sliderule-icesat2/releases/tag/v1.1.0](https://github.com/ICESat2-SlideRule/sliderule-icesat2/releases/tag/v1.1.0)

[https://github.com/ICESat2-SlideRule/sliderule-python/releases/tag/v1.1.0](https://github.com/ICESat2-SlideRule/sliderule-python/releases/tag/v1.1.0)

