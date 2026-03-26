# Release v5.3.x

2025-03-12

Version description of the v5.3.0 release of SlideRule Earth.

## New Functionality

* **v5.3.0** - [#598](https://github.com/SlideRuleEarth/sliderule/pull/598) - Added `h5x` endpoint for GeoDataFrames of arbitrary h5 resources

* **v5.3.0** - [#599](https://github.com/SlideRuleEarth/sliderule/pull/598) - Added `atl09_fields` parameter for ATL09 sampling for `atl03x`, `atl06x`, `atl08x`, and `atl13x`

* **v5.3.0** - [#600](https://github.com/SlideRuleEarth/sliderule/pull/600) - Authenticator supports `affiliate` organization role

* **v5.3.0** - Clusters support user service capacity requests; authorized users can request an autoscaling group of nodes on the public cluster dedicated just to them, similar to how a private cluster works, but going through the public load balancer

## Issues Resolved

* **v5.3.0** - [2e25ecf](https://github.com/SlideRuleEarth/sliderule/commit/2e25ecf3007bbd417e2aae00ff4f5162de10b65e) - ATL09 sampling now uses delta_time only for bckgrd_atlas group and uses segment_id for high_rate and low_rate groups.

## Development Updates

* **v5.3.0** - [4c6ca35](https://github.com/SlideRuleEarth/sliderule/commit/4c6ca35dd6b597c8ea4ad750deaae7602e6a478e) - removed hard coded host zone in certbot yml

* **v5.3.0** - [4c6ca35](https://github.com/SlideRuleEarth/sliderule/commit/4c6ca35dd6b597c8ea4ad750deaae7602e6a478e) - improved user friendliness of makefile cluster target outputs

* **v5.3.0** - [4c6ca35](https://github.com/SlideRuleEarth/sliderule/commit/4c6ca35dd6b597c8ea4ad750deaae7602e6a478e) - trimmed the package list from the version output

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