# Release v5.3.x

2025-03-12

Version description of the v5.3.0 release of SlideRule Earth.

## New Functionality

* **v5.3.0** - [#598](https://github.com/SlideRuleEarth/sliderule/pull/598) - Added `h5x` endpoint for GeoDataFrames of arbitrary h5 resources

* **v5.3.0** - [#599](https://github.com/SlideRuleEarth/sliderule/pull/598) - Added `atl09_fields` parameter for ATL09 sampling for `atl03x`, `atl06x`, `atl08x`, and `atl13x`

* **v5.3.0** - [#600](https://github.com/SlideRuleEarth/sliderule/pull/600) - Authenticator supports `affiliate` organization role

* **v5.3.0** - Clusters support user service capacity requests; authorized users can request an autoscaling group of nodes on the public cluster dedicated just to them, similar to how a private cluster works, but going through the public load balancer

* **v5.3.0** - [2e25ecf](https://github.com/SlideRuleEarth/sliderule/commit/2e25ecf3007bbd417e2aae00ff4f5162de10b65e) - added `segment_id` to atl03x dataframe

* **v5.3.1** - [b5e47e9](https://github.com/SlideRuleEarth/sliderule/commit/b5e47e9530f70439f8fff12d21625df0f78a3f08) - SurfaceBlanket runner `iceat2.blanket` provides ground and canopy heights for ATL03 data

* **v5.3.1** - [4c87b9e](https://github.com/SlideRuleEarth/sliderule/commit/4c87b9e39fad93326968d8879276f7df093b2653) - Deduplicator frame runner `dedup` provided for remove duplicate rows in dataframes

* **v5.3.1** - ATL13 AMS support upgraded to release 007, along with internal optimizations and full support for geospatial queries

## Issues Resolved

* **v5.3.0** - [2e25ecf](https://github.com/SlideRuleEarth/sliderule/commit/2e25ecf3007bbd417e2aae00ff4f5162de10b65e) - ATL09 sampling now uses delta_time only for bckgrd_atlas group and uses segment_id for high_rate and low_rate groups.

* **v5.3.1** - [581a221](https://github.com/SlideRuleEarth/sliderule/commit/581a221952b7da58546a3dc36b5dc16868eec492) - Orchestrator fix for service names that are break prometheus metric collection

* **v5.3.1** - [2c55f19](https://github.com/SlideRuleEarth/sliderule/commit/2c55f19ccd1c0d807ace631b349570f9d3dbd3f7), [514c01f](https://github.com/SlideRuleEarth/sliderule/commit/514c01fd234780b7c7fa8f72189653adbe47015d) - User service fixes and documentation

* **v5.3.1** - [f5c172b](https://github.com/SlideRuleEarth/sliderule/commit/f5c172b6989029ecb3404a31f36bdf9f0fb31050) - `h5x` to use NAME_INDEX_OPTION

* **v5.3.1** - [4c87b9e](https://github.com/SlideRuleEarth/sliderule/commit/4c87b9e39fad93326968d8879276f7df093b2653) - base64 encoding support for standard and URL encodings

* **v5.3.1** - [bd0e9a8](https://github.com/SlideRuleEarth/sliderule/commit/bd0e9a82f6ddceb70c5bbfdb24aa224143ebe9f8) - ATL09 sampler alignment fixes

* **v5.3.1** - [#603](https://github.com/SlideRuleEarth/sliderule/issues/603) - support for trailing slashes in `h5x` group names; enhanced h5coro docs

## Development Updates

* **v5.3.0** - [4c6ca35](https://github.com/SlideRuleEarth/sliderule/commit/4c6ca35dd6b597c8ea4ad750deaae7602e6a478e) - removed hard coded host zone in certbot yml

* **v5.3.0** - [4c6ca35](https://github.com/SlideRuleEarth/sliderule/commit/4c6ca35dd6b597c8ea4ad750deaae7602e6a478e) - improved user friendliness of makefile cluster target outputs

* **v5.3.0** - [4c6ca35](https://github.com/SlideRuleEarth/sliderule/commit/4c6ca35dd6b597c8ea4ad750deaae7602e6a478e) - trimmed the package list from the version output

* **v5.3.1** - Third-party support for ChatGPT authorization

* **v5.3.1** - Job runner updates and fixes

## Getting This Release

[https://github.com/SlideRuleEarth/sliderule/releases/tag/v5.3.0](https://github.com/SlideRuleEarth/sliderule/releases/tag/v5.3.0)

## Metrics

> clients/python/utils/benchmark.py
```
atl06_aoi <776718 x 16> - 30.087476 secs
atl06_ancillary <916 x 17> - 3.353301 secs
atl03_ancillary <916 x 17> - 2.959630 secs
atl06_parquet <1600 x 18> - 2.908771 secs
atl03_parquet <23072 x 23> - 1.624614 secs
atl06_sample_landsat <916 x 20> - 64.363435 secs
atl06_sample_zonal_arcticdem <1695 x 27> - 4.863028 secs
atl06_sample_nn_arcticdem <1695 x 20> - 4.558551 secs
atl06_msample_nn_arcticdem <1695 x 20> - 4.688760 secs
atl06_no_sample_arcticdem <1695 x 16> - 2.671152 secs
atl03_rasterized_subset <51832 x 22> - 2.950499 secs
atl03_polygon_subset <50615 x 22> - 2.139540 secs
```

> clients/python/utils/baseline.py
```
GEDI / 3DEP = 2661.4238434013237
ICESat-2 / ArcticDEM = 1598.2301327720206
ICESat-2 / ATL06p = 1809.893690173468
ICESat-2 / PhoREAL = 3.245903730392456
```