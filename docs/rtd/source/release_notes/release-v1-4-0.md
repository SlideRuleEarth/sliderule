# Release v1.4.0

2022-03-03

Version description of the v1.4.0 release of ICESat-2 SlideRule.

> :warning: This update breaks backward compatibility

## Required Updates for v1.4.0

* In order to use the latest SlideRule server deployments, the Python client must be updated.

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

* User scripts that use the Python client need to make the following updates:
  - The `track` keyword argument of **atl03sp**, **atl03s**, **atl06p**, and **atl06** has moved to the `parm` dictionary
  - The `block` keywork argument of **atl06p** and **atl03sp** has been removed

* User scripts that use the Python client should make the following updates due to deprecated functionality:
  - The object returned from the **icesat2.toregion** function is now a dictionary instead of a list;  the polygon should be accessed via `["poly"]` instead of with a numerical index.  If the region of interest contains multiple polygons, the convex hull of those polygons is returned.  For compatibility, for this version only, the returned polygon is also accessible at the `[0]` index.


## New Features

- Support for large regions and complex polygon requests (via providing a rasterized polygon as part of the request) [sliderule-python#41](https://github.com/ICESat2-SlideRule/sliderule-python/issues/41) and [sliderule-python#24](https://github.com/ICESat2-SlideRule/sliderule-python/issues/24)

- Added quality flags to request parameters and response fields

- Enhanced widgets example notebook with voila demo

- Parallel processing APIs (`atl06p`, and `atl03sp`) support a callback function on each granule's results

- Updated and improved documentation


## Major Issues Resolved

- Moved all traffic to port 80; compatible with mybinder [sliderule-python#48](https://github.com/ICESat2-SlideRule/sliderule-python/issues/48)

- Fixed issue when saving NetCDF files [sliderule-python#75](https://github.com/ICESat2-SlideRule/sliderule-python/issues/75)

- Updated Python dependencies [sliderule-python#53](https://github.com/ICESat2-SlideRule/sliderule-python/issues/53)


## Minor Issues Resolved

- Documented the `t0`, and `t1` request parameters

- Added `rgt`, `cycle`, and `region` request parameters [sliderule-python#27](https://github.com/ICESat2-SlideRule/sliderule-python/issues/27)

- Moved `track` request parameter from being a keyward argument to a member of the `parm` dictionary

- The `cnf` parameter default changed to 2; the `maxi` parameter default changed to 5

- Fixed inconsistencies in installation instructions [sliderule-python#9](https://github.com/ICESat2-SlideRule/sliderule-python/issues/9)


## Getting This Release

[https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v1.4.0](https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v1.4.0)

[https://github.com/ICESat2-SlideRule/sliderule-icesat2/releases/tag/v1.4.0](https://github.com/ICESat2-SlideRule/sliderule-icesat2/releases/tag/v1.4.0)

[https://github.com/ICESat2-SlideRule/sliderule-python/releases/tag/v1.4.0](https://github.com/ICESat2-SlideRule/sliderule-python/releases/tag/v1.4.0)

