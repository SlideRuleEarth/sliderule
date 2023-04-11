# Release v3.0.2

2023-04-11

Version description of the v3.0.2 release of ICESat-2 SlideRule.

## Major Changes

- REMA strip raster sampling support enabled.  SlideRule now supports both the mosaic and strip datasets of the ArcticDEM and REMA DEMs.

- The internal git repository structure of the SlideRule project has changed and is now a monorepo.  All future GitHub Issues and Discussions are encouraged to take place under the `sliderule` repository.  The `sliderule-python` repo still exists and holds the Jupyter Notebook examples and Voila demo.
  * The sliderule-python repo Python client code is now in `sliderule/clients/python`
  * The sliderule-build-and-deploy repo infrastructure code for the cluster is now in `sliderule/targets/slideruleearth-aws`
  * The sliderule-docs repo documentation code is now in `sliderule/docs`

- Each deployed cluster (both private and public) now include their own Grafana/Prometheus/Loki monitoring stack and static Read-The-Docs documentation website.  These services are behind an authentication client which uses the SlideRule Provisioning System as an identity provider.  To get access to these new services, users must have active accounts in the SlideRule Provisioning System and be members of the cluster they are attempting to access.

- The field `delta_time` has been removed from all result records.  Result records that feed data frames have times that are now nanoseconds from the Unix epoch.  This change breaks backward comaptibility between the server and the client and requires the client to be upgraded. [#185](https://github.com/ICESat2-SlideRule/sliderule/issues/185)

## New Features

- PhoREAL/atl08 endpoint still a feature-preview

- GEDI L4A subsetting endpoint still a feature-preview

## Development Updates

- The automated test suite (pytests) now run against a deployed cluster (under the developer organization).

- There is a `bypass_dns` option when initializing the Python client which can be useful when running automated tests.  It forces the client to use the ip address returned by the Provisioning System and to bypass the DNS lookup.

- Clusters can be deployed against fully specified versions [#52](https://github.com/ICESat2-SlideRule/sliderule-build-and-deploy/issues/52)

## Issues Resolved

- PhoREAL processing bug fixes: the median ground height now uses the absolute heights, and the canopy openess calculation is now correctly implements the standard deviation

## Known Issues

- GEDI subsetting results have incorrect times [#218](https://github.com/ICESat2-SlideRule/sliderule/issues/218)

## Getting This Release

[https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v3.0.2](https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v3.0.2)

[https://github.com/ICESat2-SlideRule/sliderule-icesat2/releases/tag/v3.0.2](https://github.com/ICESat2-SlideRule/sliderule-icesat2/releases/tag/v3.0.2)

[https://github.com/ICESat2-SlideRule/sliderule-python/releases/tag/v3.0.2](https://github.com/ICESat2-SlideRule/sliderule-python/releases/tag/v3.0.2)

