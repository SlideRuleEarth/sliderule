# Release v5.0.x

2025-01-27

Version description of the v5.0.6 release of SlideRule Earth.

## Breaking Changes

* **v5.0.7** - The `atl24-s3` asset has been renamed to `icesat2-atl24` to reflect that it is now the default asset for ATL24.  This will also make the transition from local S3 storage to Earthdata Cloud seamless, as the name will not need to change when that happens.

* **v5.0.3** - The use of the ***SlideRule Provisioning System*** has been deprecated.  All accounts in the system must be replaced by GitHub accounts.  Authentication for private clusters is now handled by the ***SlideRule Authenticator*** at https://login.slideruleearth.io.
  - The Python client now supports only two authentication flows: (1) the use of a PAT key from GitHub, and (2) an interactive device-flow login to GitHub
  - Support for `.netrc` files has been removed
  - The environment variable `PS_GITHUB_TOKEN` has been renamed to `SLIDERULE_GITHUB_TOKEN` to remove the "Provisioning System" prefix identifier.
  - The `ps_username` and `ps_password` parameters in `sliderule.authenticate` have been removed.
  - The `update_available_servers` function can no longer be used to change the number of nodes in a running cluster; it can only be used to initialize the number of nodes in a cluster that is to be deployed and then status the number of nodes that are running.

* **v5.0.3** - The use of the ***SlideRule Manager*** has been deprecated.  All calls to `session.manager` should no longer be used as that functionality will cease in future releases.

* **v5.0.3** - The main Python module `sliderule` no longer creates a default session on import but requires either `sliderule.init()` or `sliderule.create_session()`.  The creation of a default session was confusing when users called `sliderule.init()` which then created a second session.  This caused odd behavior with logging because at that point two loggers exist.  Moving forward, users are encouraged to use `sliderule.create_session()` which returns a session object that can then unambiguously be used to communicate with a SlideRule cluster..

* **v5.0.3** - Raster sampling support has been optimized for x-series APIs at the cost of legacy p-series API performance.  All users are strongly encouraged to switch to x-series APIs when performing raster sampling as the old p-series APIs will take much longer now.

* **v5.0.3** - Removed the `sliderule.authenticate` function as authentication must now occur when a SlideRule session is created.

* **v5.0.2** - Polygons used for `earthdata.stac` requests no longer need to be nested lists, but are supplied in the same format as all other requests:
```Python
poly = [
    {"lat": lat1, "lon": lon1},
    {"lat": lat2, "lon": lon2},
    ...
    {"lat": lat1, "lon": lon1}
]
```

* **v5.0.2** - The `raster` field which was replaced by `region_mask` and has been deprecated is now no longer supported.

* **v5.0.2** - The `nsidc-s3` asset which was replaced by the `icesat2` asset, is no longer supported.  Using `icesat2` instead of `nsidc-s3` provides identical behavior.

* **v5.0.2** - The `bypass_dns` option in the SlideRule Python Client is no longer supported as it was not reliable.

* **v5.0.2** - Queries to AMS require all parameters for the query to be inside an object with the key matching the product being queried.  For example, ATL13 queries can no longer provide the `refid` parameter at the top level of the request json, like so:
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

## New Functionality

* **v5.0.7** - [#574](https://github.com/SlideRuleEarth/sliderule/pull/574) - Copernicus DEM 30m raster support added.

* **v5.0.7** - H5Coro has been updated to support dynamically modified H5 files (which was needed to read older ATL02 granules).  This update uses the v2 BTree name index to access link messages in the fractal heap.  Currently, this support is only provided in the `h5p` and `h5` endpoints.  In the future, this will also be available to the rest of the endpoints.

* **v5.0.7** - Added `/report/tests` API to *provisioner*; it returns the summary of the results of the last test runner run.

* **v5.0.3** - The django-based ***SlideRule Provisioning System*** running in AWS ECS has reached end-of-life and is now replaced by the pure Python-based ***SlideRule Provisioner*** running in AWS lambda.

* **v5.0.3** - The authentication services provided by the ***SlideRule Provisioning System*** have been replaced by the ***SlideRule Authenticator*** which in turn uses GitHub athentication services for account management.

* **v5.0.3** - The flask-based ***SlideRule Manager*** has been removed.  Telemetry and alert logging is now handled by the AWS firehose-based ***SlideRule Recorder***.  Rate limiting and endpoint metrics are now handled the ***SlideRule Intelligent Load Balancer***.

* **v5.0.3** - [#552](https://github.com/SlideRuleEarth/sliderule/pull/552) - Ancillary field requests now support multidimensional data.

* **v5.0.3** - [#553](https://github.com/SlideRuleEarth/sliderule/pull/553) - Added x-series APIs for ATL06 (`atl06x`) and ATL08 (`atl08x`)

* **v5.0.3** - [#562](https://github.com/SlideRuleEarth/sliderule/pull/562) - Serial-mode raster sampling has been removed.

* **v5.0.3** - [#564](https://github.com/SlideRuleEarth/sliderule/pull/564) - Added x-series APIs for GEDI04A (`gedi04ax`), GEDI02A (`gedi02ax`), and GEDI01B (`gedi01bx`)

* **v5.0.2** - ATL24 uses release 002 by default, which uses the internal Asset Metadata Service (AMS).

* **v5.0.2** - [#549](https://github.com/SlideRuleEarth/sliderule/pull/549) - `h5p` now supports slices.

* **v5.0.2** - `earthdata.py` is no longer a standalone implementation of an interface to CMR and TNM, but instead makes a request to the SlideRule cluster to execute the server-side implementations in `earth_data_query.lua`.  This consolidates the interface to these services in one place, and also provides a consistent interface between the web and Python clients.

* **v5.0.2** - Added the `3dep1m` asset which accesses the same USGS 3DEP data product but uses the internal AMS service for STAC queries.  This is an attempt to alleviate issues with inconsistent availability and functionality in The National Map (TNM) service which made using 3DEP difficult.

## Issues Resolved

* **v5.0.7** - [#569](https://github.com/SlideRuleEarth/sliderule/issues/569) - fixed CRS for GEDI dataframe endpoints

* **v5.0.7** - [2c4d5b9](https://github.com/SlideRuleEarth/sliderule/commit/2c4d5b9ffffb0cfc5233c99ec09b1c1d6b436dba) - dataframe sampler now supports the file id table

* **v5.0.7** - [242d4b8](https://github.com/SlideRuleEarth/sliderule/commit/242d4b84fd200bedd14b97d71f9f103cc7264f0e) - dataframe sampler now supports rasters with bands

* **v5.0.7** - [359f2ae](https://github.com/SlideRuleEarth/sliderule/commit/359f2ae36c1fad6a8289e50c57cd9f6ffa29ea10) - fixed authentication in node.js client

* **v5.0.7** - [040f3fd](https://github.com/SlideRuleEarth/sliderule/commit/040f3fd6f890034f66e413617b36db641ba13fd4) - resolved node.js reported vulnerability

* **v5.0.5** - [1fd0f03](https://github.com/SlideRuleEarth/sliderule/commit/1fd0f03306877e11fbc0f2c395b039d8fcfb6c5d) - fixed defaults for `atl06x` and `atl08x`

* **v5.0.5** - [e2c7cc8](https://github.com/SlideRuleEarth/sliderule/commit/e2c7cc8b379f21ea92b2f17d55b79fd4306e51bc) - project bucket passed through to cluster (fixed issue of netrc file not being found)

* **v5.0.5** - [0b1f32c](https://github.com/SlideRuleEarth/sliderule/commit/0b1f32c40ea1ce7f7867aca852b81d477d6ce6b9) - provisioner check for org membership fixed (in addition to any checks related to a JWT claim that is a json array)

* **v5.0.5** - [e8011fc](https://github.com/SlideRuleEarth/sliderule/commit/e8011fc22639addf2cddb2ecf89b72246aac0e34) - GitHub PAT key permissions reduced to minimal set

* **v5.0.5** - [e65e01f](https://github.com/SlideRuleEarth/sliderule/commit/e65e01fc004611c5445b04fdfbdb2eccea9eceeb) - "org role" header passed to requests to be used by `ace` and `execre` as opposed to using "is public"

* **v5.0.5** - [3a77550](https://github.com/SlideRuleEarth/sliderule/commit/3a77550f62af6535a815e10ad3762b8a4c23966b) - added ability to select non-nan values when pulling only a single value from a raster

* **v5.0.5** - Cluster has the correct permissions to allow firehose posts

* **v5.0.3** - [#557](https://github.com/SlideRuleEarth/sliderule/pull/557) - Optimized raster batch sampling

* **v5.0.2** - The latest SlideRule Conda-Forge package is now correctly installed when using the most updated conda resolver.

* **v5.0.2** - [6a71ca6](https://github.com/SlideRuleEarth/sliderule/commit/6a71ca6c5a0a5cf7211a0f7da32473624d1bc5b0) - `atl03sp` endpoint throttled to prevent users from making large AOI requests and taking down servers.

* **v5.0.2** - [fec547f](https://github.com/SlideRuleEarth/sliderule/commit/fec547f2c725b38cfecda9aafa787600b7fb17f5) - LAS/LAZ output will now open automatically in the Python client when `open_on_complete` is set to True.

* **v5.0.2** - Rate limiting improved - error messages displayed in web client and python client; admin capabilities added for resetting rate limiting

## Development Updates

* **v5.0.7** - Provisioner now only exposes three lambdas: `schedule`, `destroy`, and `gateway`.

* **v5.0.7** - [1edc248](https://github.com/SlideRuleEarth/sliderule/commit/1edc248eebfdb930dddb5b979a89f09a2346c280) - node.js npm publishing action updated to support new login requirements

* **v5.0.3** - [069269b](https://github.com/SlideRuleEarth/sliderule/commit/069269b66b4ea04f50f541e56d0cbd45d26e6310) - Cloudformation owns public cluster dns

* **v5.0.3** - [defb7a1](https://github.com/SlideRuleEarth/sliderule/commit/defb7a110c5b593464dde9ebbbf7c60f7a0455c7) - Docker compose files are now embedded in the cloudformation configuration files for the cluster.

* **v5.0.3** - [27214b4](https://github.com/SlideRuleEarth/sliderule/commit/27214b4f4f351d3ed539ba0fe3da45f62d159cc2) - ASAN suppression removed with upgrade to PDAL

* **v5.0.3** - Removed the `ORGANIZATION` environment variable and System Configuration setting.  It has been replaced by the use of the `CLUSTER` environment variable and setting.

* **v5.0.3** - Removed the configuration json file and made all startup parameters environment variables.

* **v5.0.3** - AreaOfInterest class consolidated across ICESat-2 and GEDI

* **v5.0.2** - [#545](https://github.com/SlideRuleEarth/sliderule/pull/545) - Substantial static analysis was performed on all packages using codex and issues were resolved.

* **v5.0.2** - Runtime Exceptions (RTE) codes updated and reorganized to support the web client being able to provide meaningful feedback to user when an error occurs.

* **v5.0.2** - [ed8c58f](https://github.com/SlideRuleEarth/sliderule/commit/ed8c58f33f4c447db69bc8ea52728fe09aeb47e0) - `free_func` removed from `msgq` design

* **v5.0.2** - [57cbcd5](https://github.com/SlideRuleEarth/sliderule/commit/57cbcd582da57372c904e1e48d2071d2b0510848) - S3 puts provide better error messages in backend logs

* **v5.0.2** - Docker containers are now all standardized on using AL2023 as the base image unless an alternative is required (e.g. ilb)

* **v5.0.2** - [e982826](https://github.com/SlideRuleEarth/sliderule/commit/e9828262adff70ff5045f1e5e89512d4a0ce190f) - The base AMI used for all cluter nodes is not the AL2023 ECS optimized image provided by AWS.

## Getting This Release

[https://github.com/SlideRuleEarth/sliderule/releases/tag/v5.0.6](https://github.com/SlideRuleEarth/sliderule/releases/tag/v5.0.6)

## Metrics

> clients/python/utils/benchmark.py
```
atl06_aoi <766208 x 16> - 29.947413 secs
atl06_ancillary <916 x 17> - 2.865666 secs
atl03_ancillary <916 x 17> - 2.525880 secs
atl06_parquet <1600 x 18> - 2.707429 secs
atl03_parquet <23072 x 23> - 1.626420 secs
atl06_sample_landsat <916 x 20> - 58.570350 secs
atl06_sample_zonal_arcticdem <1695 x 27> - 4.856738 secs
atl06_sample_nn_arcticdem <1695 x 20> - 4.646599 secs
atl06_msample_nn_arcticdem <1695 x 20> - 4.546073 secs
atl06_no_sample_arcticdem <1695 x 16> - 2.524935 secs
atl03_rasterized_subset <51832 x 22> - 2.428944 secs
atl03_polygon_subset <50615 x 22> - 2.160177 secs
```

> clients/python/utils/baseline.py
```
GEDI / 3DEP = 2653.7247077111642
ICESat-2 / ArcticDEM = 1598.2301327720206
ICESat-2 / ATL06p = 1809.893690173468
ICESat-2 / PhoREAL = 3.245903730392456
```