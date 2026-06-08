# Release v1.4.x

2022-08-01

Version description of the v1.4.6 release of ICESat-2 SlideRule.

> :warning: **v1.4.0** - This update breaks backward compatibility

> :warning: **v1.4.1** - This is the last release of the client to default to version '004' of the ICESat-2 ATL03 dataset.  The next release will default to using version '005' of the ICESat-2 ATL03 dataset.

> :warning: **v1.4.2** - This is the first release of the client to default to version '005' of the ICESat-2 ATL03 dataset.

## Required Updates

* **v1.4.0** - In order to use the latest SlideRule server deployments, the Python client must be updated.

For conda users:
```bash
$ conda update sliderule
```

For developer installs:
```bash
$ cd sliderule-python
$ git checkout main
$ git pull
$ python3 setup.py install
```

* **v1.4.0** - User scripts that use the Python client need to make the following updates:
  - The `track` keyword argument of **atl03sp**, **atl03s**, **atl06p**, and **atl06** has moved to the `parm` dictionary
  - The `block` keyword argument of **atl06p** and **atl03sp** has been removed

* **v1.4.0** - User scripts that use the Python client should make the following updates due to deprecated functionality:
  - The object returned from the **icesat2.toregion** function is now a dictionary instead of a list;  the polygon should be accessed via `["poly"]` instead of with a numerical index.  If the region of interest contains multiple polygons, the convex hull of those polygons is returned.  For compatibility, for this version only, the returned polygon is also accessible at the `[0]` index.


## New Features

- **v1.4.6** - Updated Python client to support ICESat-2's proxied APIs

- **v1.4.6** - Implemented Kmeans clustering support for large regions of interest.  See `icesat2.toregion` function and the `n_clusters` parameter.

- **v1.4.2** - Updated YAPC algorithm to Jeff's latest specification (05-04-22).  The new algorithm is the default version that runs.  If the previous algorithm is desired, there is a `version` parameter which is a part of the `yapc` parameter block that can be set to "1", and the original algorithm will execute.  Note: the new algorithm runs about three times faster than the original one.

- **v1.4.2** - Updated internal threading heuristic in the Python client:
  * client will attempt to throttle the number of concurrent requests to any given processing node
  * the ***max_workers*** parameter in the `atl06p` and `atl03sp` APIs has been removed;  if the calling application must change the number pending requests per node, then there is a new API `sliderule.set_max_pending` that can be called.

- **v1.4.2** - The `sliderule-docs` repository is now public and contains all of the SlideRule documentation used to build the http://icesat2sliderule.org website.

- **v1.4.2** - API reference documentation has been moved from Sphinx into the python function docstrings; Sphinx now uses `.. autofunction::` to pull it into the website.

- **v1.4.2** - SlideRule's Python bindings (`srpybin`) can now be built with the ICESat-2 plugin support (this enables the H5Coro to read from the Cumulus S3 archives).

- **v1.4.1** - Updated notes and documentation inside Python demos

- **v1.4.1** - Added photon classification widgets

- **v1.4.1** - Updates to `io.py` module [sliderule-python#91](https://github.com/SlideRuleEarth/sliderule-python/pull/91), [sliderule-python#93](https://github.com/SlideRuleEarth/sliderule-python/pull/93)

- **v1.4.1** - Added CredentialStore bindings to Python target: [pyCredentialStore](https://github.com/SlideRuleEarth/sliderule/blob/main/targets/binding-python/pyCredentialStore.cpp)

- **v1.4.1** - Set log retention policies in Loki, with rolling backups to S3

- **v1.4.1** - Added disk metrics for the Monitor EC2 instance to grafana

- **v1.4.1** - Country of origin statistics [sliderule-project#15](https://github.com/SlideRuleEarth/sliderule-project/issues/15)

- **v1.4.1** - Added `spot` column to the ***atl03s*** and ***atl03sp*** returned geodataframes. [177792f](https://github.com/SlideRuleEarth/sliderule-python/commit/177792fac19a3ef0ba10a73d09b6fe7f1a70c48c)

- **v1.4.0** - Support for large regions and complex polygon requests (via providing a rasterized polygon as part of the request) [sliderule-python#41](https://github.com/SlideRuleEarth/sliderule-python/issues/41) and [sliderule-python#24](https://github.com/SlideRuleEarth/sliderule-python/issues/24)

- **v1.4.0** - Added quality flags to request parameters and response fields

- **v1.4.0** - Enhanced widgets example notebook with voila demo

- **v1.4.0** - Parallel processing APIs (`atl06p`, and `atl03sp`) support a callback function on each granule's results

- **v1.4.0** - Updated and improved documentation


## Major Issues Resolved

- **v1.4.6** - Fixed bug [#106](https://github.com/SlideRuleEarth/sliderule-python/issues/106) in Python client when a node is identified in multiple concurrent threads to be removed, only the first wins and the others just pass.

- **v1.4.3** - Updated YAPC algorithm to Jeff's latest specification (05-23-22): version 3.0.  The new algorithm is the default version that runs.  If the previous algorithm is desired, there is a `version` parameter which is a part of the `yapc` parameter block that can be set to "1", and the original algorithm will execute.  Note: the new algorithm runs about three times faster than the original one.

- **v1.4.3** - Fixed calculation of the segment id.  There was a rounding error on the server side and a parsing error on the construction of the dataframe in the client. [a5db62a](https://github.com/SlideRuleEarth/sliderule-python/commit/a5db62ad9b7570e25ab7105eaec06267e4fadf11), and [ec2b1f6](https://github.com/SlideRuleEarth/sliderule-icesat2/commit/ec2b1f6bc53c4f1e93f0dcffa7abd4dcec8379c6).

- **v1.4.2** - Fixed spot calculation to match what is specified in the ATL03 ATBD. [b8ea34b](https://github.com/SlideRuleEarth/sliderule-python/commit/b8ea34bb24ea243b968de5df8f12939857393f46).

- **v1.4.2** - Fixed Voila demo points showing up 360 degrees away [#97](https://github.com/SlideRuleEarth/sliderule-python/pull/97)

- **v1.4.2** - Added significant tests to pytest framework for client: algorithm, atl08, atl06gs, h5 failure cases

- **v1.4.2** - A logging level is specified for Lua endpoints; this allows the `prometheus` and `health` endpoints to use the DEBUG log level so that the logs are not spammed.

- **v1.4.1** - Fixed multiple bugs leading to missing elevations on some processing runs. [sliderule#92](https://github.com/SlideRuleEarth/sliderule/issues/92).  A result of fixing this bug is that users now see messages like "RESOURCE NOT FOUND" when requesting granules that don't exist. [sliderule#41](https://github.com/SlideRuleEarth/sliderule/issues/41).

- **v1.4.1** - Fixed bug in H5Coro when processing chunked datasets when the chunk dimensions do not span the full number of columns. [c48b918](https://github.com/SlideRuleEarth/sliderule/commit/c48b918e12080fb97fb4bb6e3849841eadc75ed2)

- **v1.4.1** - SlideRule server code now returns an ***Exception Record***(`exceptrec`) back to the client when a processing exception occurs.  The client can use this to retry the request or provide diagnostic information to the user as to what went wrong.  The SlideRule Python client will retry the request depending on the severity of the failure. [a1a9b7b](https://github.com/SlideRuleEarth/sliderule/commit/a1a9b7b8e7a4c29e834631f50f06ddb5fa612193)

- **v1.4.0** - Moved all traffic to port 80; compatible with mybinder [sliderule-python#48](https://github.com/SlideRuleEarth/sliderule-python/issues/48)

- **v1.4.0** - Fixed issue when saving NetCDF files [sliderule-python#75](https://github.com/SlideRuleEarth/sliderule-python/issues/75)

- **v1.4.0** - Updated Python dependencies [sliderule-python#53](https://github.com/SlideRuleEarth/sliderule-python/issues/53)


## Minor Issues Resolved

- **v1.4.6** - Fixed crash in Voila demo when no photons are returned. [#101](https://github.com/SlideRuleEarth/sliderule-python/issues/101);

- **v1.4.2** - Multiple lua script bugs fixed (discovered mostly through static analysis, but some were discovered through runtime errors)

- **v1.4.2** - Optimized `List.h` template sort function

- **v1.4.2** - More robust error checking in `netsvc` package _post_ and _get_ functions (the http response code is checked explicitly).

- **v1.4.2** - Fixed CPU usage metrics in Grafana [3a5fd92](https://github.com/SlideRuleEarth/sliderule-project/commit/3a5fd92178c3028c1fcd5d6b3c08ff0e2fff1758)

- **v1.4.1** - Voila demo updates:
    - Increased maximum number of resources that can be processed in a request to 1,000
    - Handle case where no results are returned
    - Handle case where user does not draw a polygon before making a request
    - Use updated ipysliderule widgets for RGT, GT, and cycle
    - Fixed progress message to accurately report number of returned elevations per resource

- **v1.4.1** - Updated pistache to latest release [sliderule#86](https://github.com/SlideRuleEarth/sliderule/issues/86)

- **v1.4.1** - The day and the month is reported in log messages instead of the day of year.  This was implemented in a previous release, but was missed and is now being included in these release notes for completeness.  [sliderule#69](https://github.com/SlideRuleEarth/sliderule/issues/69)

- **v1.4.0** - Documented the `t0`, and `t1` request parameters

- **v1.4.0** - Added `rgt`, `cycle`, and `region` request parameters [sliderule-python#27](https://github.com/SlideRuleEarth/sliderule-python/issues/27)

- **v1.4.0** - Moved `track` request parameter from being a keyword argument to a member of the `parm` dictionary

- **v1.4.0** - The `cnf` parameter default changed to 2; the `maxi` parameter default changed to 5

- **v1.4.0** - Fixed inconsistencies in installation instructions [sliderule-python#9](https://github.com/SlideRuleEarth/sliderule-python/issues/9)


## Getting This Release

[https://github.com/SlideRuleEarth/sliderule/releases/tag/v1.4.6](https://github.com/SlideRuleEarth/sliderule/releases/tag/v1.4.6)

[https://github.com/SlideRuleEarth/sliderule-icesat2/releases/tag/v1.4.6](https://github.com/SlideRuleEarth/sliderule-icesat2/releases/tag/v1.4.6)

[https://github.com/SlideRuleEarth/sliderule-python/releases/tag/v1.4.6](https://github.com/SlideRuleEarth/sliderule-python/releases/tag/v1.4.6)
