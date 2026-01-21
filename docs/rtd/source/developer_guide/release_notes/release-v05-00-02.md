# Release v5.0.2

2025-01-20

Version description of the v5.0.2 release of SlideRule Earth.

## Breaking Changes

* The use of the ***SlideRule Provisioning System*** has been deprecated.  All accounts in the system must be replaced by GitHub accounts.  Authentication for private clusters is now handled by the ***SlideRule Authenticator*** at https://login.slideruleearth.io.
  - The Python client now supports only two authentication flows: (1) the use of a PAT key from GitHub, and (2) an interactive device-flow login to GitHub
  - Support for `.netrc` files has been removed
  - The environment variable `PS_GITHUB_TOKEN` has been renamed to `SLIDERULE_GITHUB_TOKEN` to remove the "Provisioning System" prefix identifier.
  - The `ps_username` and `ps_password` parameters in `sliderule.authenticate` have been removed.
  - The `update_available_servers` function can no longer be used to change the number of nodes in a running cluster; it can only be used to initialize the number of nodes in a cluster that is to be deployed and then status the number of nodes that are running.

* The use of the ***SlideRule Manager*** has been deprecated.  All calls to `session.manager` should no longer be used as that functionality will cease in future releases.

* The main Python module `sliderule` no longer creates a default session on import but requires either `sliderule.init()` or `sliderule.create_session()`.  The creation of a default session was confusing when users called `sliderule.init()` which then created a second session.  This caused odd behavior with logging because at that point two loggers exist.  Moving forward, users are encouraged to use `sliderule.create_session()` which returns a session object that can then unambigiously be used to communicate with a SlideRule cluster..

* Raster sampling support has been optimized for x-series APIs at the cost of legacy p-series API performance.  All users are strongly encouraged to switch to x-series APIs when performing raster sampling as the old p-series APIs will take much longer now.

* Removed the `sliderule.authenticate` function as authentication must now occur when a SlideRule session is created.

* [v5.0.2](/web/rtd/developer_guide/release_notes/release-v05-00-00.html) - Polygons used for `earthdata.stac` requests no longer need to be nested lists, but are supplied in the same format as all other requests:
```Python
poly = [
    {"lat": lat1, "lon": lon1},
    {"lat": lat2, "lon": lon2},
    ...
    {"lat": lat1, "lon": lon1}
]
```

* [v5.0.2](/web/rtd/developer_guide/release_notes/release-v05-00-00.html) - The `raster` field which was replaced by `region_mask` and has been deprecated is now no longer supported.

* [v5.0.2](/web/rtd/developer_guide/release_notes/release-v05-00-00.html) - The `nsidc-s3` asset which was replaced by the `icesat2` asset, is no longer supported.  Using `icesat2` instead of `nsidc-s3` provides identical behavior.

* [v5.0.2](/web/rtd/developer_guide/release_notes/release-v05-00-00.html) - The `bypass_dns` option in the SlideRule Python Client is no longer supported as it was not reliable.

* [v5.0.2](/web/rtd/developer_guide/release_notes/release-v05-00-00.html) - Queries to AMS require all parameters for the query to be inside an object with the key matching the product being queried.  For example, ATL13 queries can no longer provide the `refid` parameter at the top level of the request json, like so:
```json
{
    "refid": 110234231
}
```
Instead the request json must change to be this:
```json
{
    "atl13": {
        "refid": 110234231
    }
}
```

## Major Changes & New Functionality

* The django-based ***SlideRule Provisioning System*** running in AWS ECS has reached end-of-life and is now replaced by the pure Python-based ***SlideRule Provisioner*** running in AWS lambda.

* The authentication services provided by the ***SlideRule Provisioning System*** have been replaced by the ***SlideRule Authenticator*** which in turn uses GitHub athentication services for account management.

* The flask-based ***SlideRule Manager*** has been removed.  Telemetry and alert logging is now handled by the AWS firehose-based ***SlideRule Recorder***.  Rate limiting and endpoint metrics are now handled the ***SlideRule Intelligent Load Balancer***.

* [#552](https://github.com/SlideRuleEarth/sliderule/pull/552) - Ancillary field requests now support multidimensional data.

* [#553](https://github.com/SlideRuleEarth/sliderule/pull/553) - Added x-series APIs for ATL06 (`atl06x`) and ATL08 (`atl08x`)

* [#562](https://github.com/SlideRuleEarth/sliderule/pull/562) - Serial-mode raster sampling has been removed.

* [#564](https://github.com/SlideRuleEarth/sliderule/pull/564) - Added x-series APIs for GEDI04A (`gedi04ax`), GEDI02A (`gedi02ax`), and GEDI01B (`gedi01bx`)

## Known Issues

* The GitHub actions related to running the pytests and the selftest are currently not working.  This is due to them being transitioned to the provisioning system.

## Issues Resolved

* [#557](https://github.com/SlideRuleEarth/sliderule/pull/557) - Optimized raster batch sampling

## Development Changes

* [069269b](https://github.com/SlideRuleEarth/sliderule/commit/069269b66b4ea04f50f541e56d0cbd45d26e6310) - Cloudformation owns public cluster dns

* [defb7a1](https://github.com/SlideRuleEarth/sliderule/commit/defb7a110c5b593464dde9ebbbf7c60f7a0455c7) - Docker compose files are now embedded in the cloudformation configuration files for the cluster.

* [27214b4](https://github.com/SlideRuleEarth/sliderule/commit/27214b4f4f351d3ed539ba0fe3da45f62d159cc2) - ASAN suppression removed with upgrade to PDAL

* Removed the `ORGANIZATION` environment variable and System Configuration setting.  It has been replaced by the use of the `CLUSTER` environment variable and setting.

* Removed the configuration json file and made all startup parameters environment variables.

* AreaOfInterest class consolidated across ICESat-2 and GEDI

## Getting This Release

[https://github.com/SlideRuleEarth/sliderule/releases/tag/v5.0.2](https://github.com/SlideRuleEarth/sliderule/releases/tag/v5.0.2)

## Benchmarks
> clients/python/utils/benchmark.py
```
```

## Baseline
> clients/python/utils/baseline.py
```
```