# Release v3.1.0

2023-04-27

Version description of the v3.1.0 release of ICESat-2 SlideRule.

## Major Changes

- GEDI functionality officially supported
  * Subsetting for L1B, L2A, L4A datasets (L1 and L2 products limited to Grand Mesa, Colorado area of interest until LP DAAC migrates them to the cloud)
  * Raster Sampling for L3, L4B datasets
  * User Guide: https://slideruleearth.io/web/rtd/user_guide/GEDI.html
  * API Reference: https://slideruleearth.io/web/rtd/api_reference/gedi.html
  * Example Notebooks: https://github.com/ICESat2-SlideRule/sliderule-python/tree/main/examples

- PhoREAL functionality officially supported
  * User Guilde: https://slideruleearth.io/web/rtd/user_guide/ICESat-2.html#photon-extent-parameters
  * API Reference: https://slideruleearth.io/web/rtd/api_reference/icesat2.html#atl08p
  * Example Notebooks: https://slideruleearth.io/web/rtd/getting_started/Examples.html (look for `PhoREAL Example`)

## Development Updates

- Now using OpenID Connect protocol to authenticate users trying to access cluster instances of grafana and static website.  

- Direct support/option in provisioning system for deploying public clusters

- Overhaul of asset management - each asset now has an identity that is used for authentication; multiple assets can have the same identity so that a single set of credentials can be applied to all of them.

## Issues Resolved

- Progress messages in demo appear in order [#190](https://github.com/ICESat2-SlideRule/sliderule/issues/190)

- GEDI timestamps fixed [#218](https://github.com/ICESat2-SlideRule/sliderule/issues/218)

- Fixed netCDF and HDF5 output from Python I/O module for delta_time update [a2e281a](https://github.com/ICESat2-SlideRule/sliderule/commit/a2e281a744bfb943c0ab04e4e1b3b603b1c58afc)

## Getting This Release

[https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v3.1.0](https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v3.1.0)

[https://github.com/ICESat2-SlideRule/sliderule-icesat2/releases/tag/v3.1.0](https://github.com/ICESat2-SlideRule/sliderule-icesat2/releases/tag/v3.1.0)

[https://github.com/ICESat2-SlideRule/sliderule-python/releases/tag/v3.1.0](https://github.com/ICESat2-SlideRule/sliderule-python/releases/tag/v3.1.0)

