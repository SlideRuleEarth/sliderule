# Release v4.3.2

2024-04-04

Version description of the v4.3.2 release of SlideRule Earth.

## New Features

* Container Runtime Environment (__Feature Preview__) - ability to run Python containers from SlideRule endpoint

* CSV output from ParquetBuilder supported

## Issues Resolved

* [8c8fe17](https://github.com/SlideRuleEarth/sliderule/commit/8c8fe174e25fff0fb7fd6416cbf25b6bd63580d3) - docker compose restart policy changed to "always" for production

* [5d893d0](https://github.com/SlideRuleEarth/sliderule/commit/5d893d0769425f4178304960ad0148fc62483919) - fixed bug in atl08 processing that caused crash when last batch was a full batch

* [35c071f](https://github.com/SlideRuleEarth/sliderule/commit/35c071fadf9a14d58c95b66b4e578cdb05bcab3c) - changed health check to go to AppServer and removed ProbeServer; this fixes the issue where the AppServer goes down but the ProbeServer remained healthy and therefore the node was deemed healthy

* [35c071f](https://github.com/SlideRuleEarth/sliderule/commit/35c071fadf9a14d58c95b66b4e578cdb05bcab3c) - fixed bug in S3 curl library where S3 get request immediately failed and debug message attempted to be printed with no data

* [08a3eaf](https://github.com/SlideRuleEarth/sliderule/commit/08a3eaf9ac1758c097e60a81f47d4029fa1ab6cd) - implemented proxy (EnpointProxy) retries for failed proxied requests; the lock for the proxied node is held until the request is successful or the retries are exhausted (this prevents the retry always going back to the same node); polling for available nodes was slowed down to 1Hz (from 5Hz); bug in cleaning up ancillary data fixed)

* [01f5b8b](https://github.com/SlideRuleEarth/sliderule/commit/01f5b8b7c79bdd248029b326d4e8b9dac971f5b3) - pinned v15.0.2 of Apache Arrow library to avoid memory bug in latest tag; (the arrow repo does not have releases)

* [5338ef6](https://github.com/SlideRuleEarth/sliderule/commit/5338ef6f6166a15b1ab6b83abe3632b2d34790c3) - fixed CurlLib memory leak when client request is prematurely terminated

* [cf5290a](https://github.com/SlideRuleEarth/sliderule/commit/cf5290a3007b95c922d6e03f2c8c52d73e8370f0) - added memory usage limit to health check to avoid situation where a node with not enough memory to service a processing request keeps registering itself (and consequently always has no active locks because it rejects all requests and therefore keeps having requests routed to it)

## Development Updates

* [f68aa36](https://github.com/SlideRuleEarth/sliderule/commit/f68aa367b4436f528ea40b902d4d621288a18024) - number of threads in proxy (EndpointProxy) is calculated based on a user provided hint or the number of nodes in the cluster currently registered

* [08a3eaf](https://github.com/SlideRuleEarth/sliderule/commit/08a3eaf9ac1758c097e60a81f47d4029fa1ab6cd) - added support for stack traces to have symbols from dynamically loaded modules (via `__no_unload__` which is defaulted in Debug builds)

* [5338ef6](https://github.com/SlideRuleEarth/sliderule/commit/5338ef6f6166a15b1ab6b83abe3632b2d34790c3) - `sys.lsobj` Lua API added to list global LuaObjects and total number of LuaObjects

## Getting This Release

[https://github.com/SlideRuleEarth/sliderule/releases/tag/v4.3.2](https://github.com/SlideRuleEarth/sliderule/releases/tag/v4.3.2)

## Benchmarks
```
atl06_aoi
        output:              625445 x 16 elements
        total:               26.511650 secs
        gdf2poly:            0.000324 secs
        toregion:            0.094860 secs
        __parse_native:      21.172510 secs
        todataframe:         0.391770 secs
        merge:               0.000002 secs
        flatten:             2.783902 secs
        atl06p:              26.266139 secs
atl06_ancillary
        output:              1180 x 17 elements
        total:               3.049564 secs
        gdf2poly:            0.000324 secs
        toregion:            0.094860 secs
        __parse_native:      0.690858 secs
        todataframe:         0.005812 secs
        merge:               0.013563 secs
        flatten:             0.030884 secs
        atl06p:              2.857920 secs
atl03_ancillary
        output:              1180 x 17 elements
        total:               3.047120 secs
        gdf2poly:            0.000324 secs
        toregion:            0.094860 secs
        __parse_native:      0.158048 secs
        todataframe:         0.005754 secs
        merge:               0.013669 secs
        flatten:             0.031304 secs
        atl06p:              3.045915 secs
atl06_parquet
        output:              1577 x 17 elements
        total:               2.755643 secs
        gdf2poly:            0.000324 secs
        toregion:            0.094860 secs
        __parse_native:      0.209622 secs
        todataframe:         0.005754 secs
        merge:               0.013669 secs
        flatten:             0.044833 secs
        atl06p:              2.755176 secs
atl03_parquet
        output:              22833 x 22 elements
        total:               1.633988 secs
        gdf2poly:            0.000324 secs
        toregion:            0.094860 secs
        __parse_native:      0.011271 secs
        todataframe:         0.005754 secs
        merge:               0.013669 secs
        flatten:             0.044833 secs
        atl06p:              2.755176 secs
        atl03sp:             1.607882 secs
atl06_sample_landsat
        output:              914 x 20 elements
        total:               7.073046 secs
        gdf2poly:            0.000324 secs
        toregion:            0.094860 secs
        __parse_native:      0.370676 secs
        todataframe:         0.006028 secs
        merge:               0.012087 secs
        flatten:             0.031790 secs
        atl06p:              4.672313 secs
        atl03sp:             1.607882 secs
atl06_sample_zonal_arcticdem
        output:              1651 x 27 elements
        total:               6.384218 secs
        gdf2poly:            0.000320 secs
        toregion:            0.006032 secs
        __parse_native:      0.319357 secs
        todataframe:         0.005968 secs
        merge:               0.020238 secs
        flatten:             0.045042 secs
        atl06p:              6.375683 secs
        atl03sp:             1.607882 secs
atl06_sample_nn_arcticdem
        output:              1651 x 20 elements
        total:               4.758471 secs
        gdf2poly:            0.000278 secs
        toregion:            0.005407 secs
        __parse_native:      0.091373 secs
        todataframe:         0.005742 secs
        merge:               0.018872 secs
        flatten:             0.038760 secs
        atl06p:              4.751282 secs
        atl03sp:             1.607882 secs
atl06_msample_nn_arcticdem
        output:              1596050 x 20 elements
        total:               143.910649 secs
        gdf2poly:            0.000268 secs
        toregion:            0.005141 secs
        __parse_native:      89.897940 secs
        todataframe:         0.975277 secs
        merge:               15.259323 secs
        flatten:             29.145164 secs
        atl06p:              142.751770 secs
        atl03sp:             1.607882 secs
atl06_no_sample_arcticdem
        output:              1596050 x 16 elements
        total:               69.063557 secs
        gdf2poly:            0.000306 secs
        toregion:            0.006468 secs
        __parse_native:      53.275093 secs
        todataframe:         0.964930 secs
        merge:               0.000003 secs
        flatten:             7.419176 secs
        atl06p:              67.920378 secs
        atl03sp:             1.607882 secs
atl03_rasterized_subset
        output:              51968 x 22 elements
        total:               4.176219 secs
        gdf2poly:            0.000306 secs
        toregion:            0.006468 secs
        __parse_native:      1.858839 secs
        todataframe:         0.042089 secs
        merge:               0.000003 secs
        flatten:             0.371834 secs
        atl06p:              67.920378 secs
        atl03sp:             3.623235 secs
atl03_polygon_subset
        output:              50799 x 22 elements
        total:               3.140495 secs
        gdf2poly:            0.000306 secs
        toregion:            0.006468 secs
        __parse_native:      1.370758 secs
        todataframe:         0.041492 secs
        merge:               0.000003 secs
        flatten:             0.359666 secs
        atl06p:              67.920378 secs
        atl03sp:             3.114903 secs
```

## Metrics
```
gedi04a_count 1507918
atl06_count 87138
atl08_count 10885
atl06s_count 3480
atl03s_count 2105

num_requests 383476
num_complete 1611955
num_failures 66
num_timeouts 113
num_active_locks 0
sliderule_members 7
atl03sp_count 1511
gedi01bp_count 2
atl06p_count 1331
health_count 560355
gedi02ap_count 5
gedi01b_count 2
time_count 10
h5p_count 174
atl08p_count 156
subsets_count 3
geo_count 4
gedi02a_count 279
prometheus_count 563886
definition_count 2602
gedi04ap_count 110152
version_count 1157
atl06sp_count 38
samples_count 9
gedi02a_sum 537.320136
version 0.001322
atl06 11.109224
atl06_sum 269222.302717
atl03s_sum 45790.410039
health 0.000952
samples_sum 52.696181
gedi02a 0.105243
atl03sp 0.001261
atl06p 12.204205
prometheus_sum 351.784345
atl08p_sum 7718.444294
gedi04ap 3.112503
gedi04a_sum 679848.393561
atl06p_sum 25904.344663
definition 0.001652
atl06s 2.181776
subsets 11.677106
atl08 27.497538
atl08p 28.554842
atl08_sum 77180.547501
gedi04a 2.039029
time_sum 0.009534
atl06sp 4.233367
atl06s_sum 5772.677444
atl03s 1.296792
h5p_sum 531.007692
atl06sp_sum 1227.984701
h5p 1.226820
geo_sum 0.004317
atl03sp_sum 14648.312617
geo 0.000992
definition_sum 6.065591
gedi02ap_sum 47.440978
health_sum 519.067018
gedi01bp_sum 9.028753
version_sum 1.458390
gedi01bp 4.113071
gedi01b_sum 6.861664
gedi01b 3.025415
prometheus 0.000528
gedi04ap_sum 338562.337289
subsets_sum 27.980504
gedi02ap 1.210191
samples 2.317266
time 0.000976
```

## Version String
```json
{
        "usgs3dep": {
                "commit":"v4.3.2-0-g63f4421c",
                "version":"v4.3.2"
        },
        "swot": {
                "commit":"v4.3.2-0-g63f4421c",
                "version":"v4.3.2"
        },
        "landsat": {
                "commit":"v4.3.2-0-g63f4421c",
                "version":"v4.3.2"
        },
        "pgc": {
                "commit":"v4.3.2-0-g63f4421c",
                "version":"v4.3.2"
        },
        "server": {
                "version":"v4.3.2",
                "commit":"v4.3.2-0-g63f4421c",
                "packages":["core","arrow","aws","cre","geo","h5","netsvc","gedi","icesat2","landsat","opendata","pgc","swot","usgs3dep"],
                "launch":"2024-04-14T06:15:44Z",
                "environment":"v4.3.2-0-g63f4421c",
                "duration":1665622349
        },
        "icesat2": {
                "commit":"v4.3.2-0-g63f4421c",
                "version":"v4.3.2"
        },
        "gedi": {
                "commit":"v4.3.2-0-g63f4421c",
                "version":"v4.3.2"
        },
        "opendata": {
                "commit":"v4.3.2-0-g63f4421c",
                "version":"v4.3.2"
        }
}
```