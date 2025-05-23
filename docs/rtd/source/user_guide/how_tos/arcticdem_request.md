# Including ArcticDEM Samples

2022-12-23


## Overview

This tutorial walks you through the steps necessary to sample the ArcticDEM at the location of each elevation generated by an `atl06p` request.  The DEM value is returned as an additional column in the GeoDataFrame.

For a full set of code associated with this tutorial, see: https://github.com/SlideRuleEarth/sliderule-python/blob/development/examples/arcticdem.ipynb.

**Prerequisites**: This walk-through assumes you have already installed the SlideRule Python client and familiar with how to use it. See the [installation](/getting_started/Install) instructions in the reference documentation if you need help installing the SlideRule Python client.  See the [Making Your First Request](./first_request) tutorial if you've never made a SlideRule processing request before.


## Background

The ArcticDEM is a high-resolution digital surface model of the Arctic, produced by the Polar Geospatial Center at the University of Minnesota, and hosted in a public S3 bucket in the AWS us-west-2 data center. SlideRule is able to associate a elevations sampled from the ArcticDEM with each elevation generated from ICESat-2's photon cloud data.  The combined dataset provides a side-by-side comparison of ATL06-SR elevations with ArcticDEM elevations.


## Sampling the ArcticDEM mosaic in an `atl06p` request

The __"samples"__ parameter is used to request ArcticDEM samples be included in `atl06p` responses.  For the ArcticDEM, there are two possible values that can be provided: _"arcticdem-mosaic"_ and _"arcticdem-strips"_.


__Step 1__: Import and initialize the SlideRule Python package for ICESat-2.
```python
>>> from sliderule import sliderule, icesat2
>>> icesat2.init("slideruleearth.io")
```

__Step 2__: Create parameters for a typical `atl06p` processing request.
```python
>>> grand_mesa = sliderule.toregion('dicksonfjord.geojson')
>>> parms = {
    "poly": grand_mesa["poly"],
    "srt": icesat2.SRT_LAND,
    "cnf": icesat2.CNF_SURFACE_HIGH,
    "len": 40.0,
    "res": 20.0
}
```
The **dicksonfjord.geojson** file used in this example can be downloaded by navigating to our [downloads](./downloads) page; alternatively, you can create your own GeoJSON file at [geojson.io](https://geojson.io).  Be sure that your region of interest is in the arctic, otherwise there will be no data in the ArcticDEM for it.

__Step 3__: Specify sampling the ArcticDEM mosaic.
```python
>>> parms["samples"] = {"mosaic": {"asset": "arcticdem-mosaic", "radius": 10.0, "zonal_stats": True}}
```

__Step 4__: Issue the processing request to SlideRule, for only a single granule (to keep the data volume down).
```python
>>> resource = "ATL03_20190314093716_11600203_005_01.h5"
>>> gdf = icesat2.atl06p(parms, resources=[resource])
```
When this completes (~10 seconds), the _gdf_ variable should now contain all of the results of the elevations calculated by SlideRule with a corresponding column for the "arcticdem-mosaic".  Note that the granule provided must be in the region of interest.

__Step 5__: Display a summary of the results.
```python
>>> print(gdf)

	segment_id 	rgt 	n_fit_photons 	spot 	h_sigma 	gt 	delta_time 	cycle 	rms_misfit 	dh_fit_dy 	... 	geometry 	mosaic.time 	mosaic.max 	mosaic.mad 	mosaic.mean 	mosaic.median 	mosaic.value 	mosaic.count 	mosaic.stdev 	mosaic.min
time
2019-03-14 09:40:46.282934432 	405226 	1160 	12 	1 	0.551360 	10 	3.779165e+07 	2 	0.874532 	0.0 	... 	POINT (-26.27920 72.77984) 	1.176077e+09 	558.497131 	2.760759 	550.909552 	550.701904 	550.909729 	81 	3.328835 	544.854675
2019-03-14 09:40:46.284351344 	405227 	1160 	32 	1 	0.246645 	10 	3.779165e+07 	2 	1.308783 	0.0 	... 	POINT (-26.27924 72.77993) 	1.176077e+09 	568.031189 	4.222864 	559.327384 	559.479126 	559.673889 	81 	4.824281 	551.360779
2019-03-14 09:40:46.285767320 	405227 	1160 	20 	1 	1.144088 	10 	3.779165e+07 	2 	1.318949 	0.0 	... 	POINT (-26.27928 72.78002) 	1.176077e+09 	573.887756 	3.402086 	566.768132 	567.251282 	567.251282 	81 	4.031628 	558.077698
2019-03-14 09:40:46.291444880 	405229 	1160 	15 	1 	0.187902 	10 	3.779165e+07 	2 	0.263953 	0.0 	... 	POINT (-26.27943 72.78037) 	1.176077e+09 	599.084351 	1.593114 	594.913225 	594.901855 	594.901855 	81 	1.911160 	591.075134
2019-03-14 09:40:46.292858704 	405230 	1160 	27 	1 	0.059746 	10 	3.779165e+07 	2 	0.269889 	0.0 	... 	POINT (-26.27947 72.78046) 	1.176077e+09 	602.492004 	1.780210 	598.034786 	597.679504 	597.673462 	81 	2.093415 	594.528076
... 	... 	... 	... 	... 	... 	... 	... 	... 	... 	... 	... 	... 	... 	... 	... 	... 	... 	... 	... 	... 	...
2019-03-14 09:40:48.353273584 	405838 	1160 	27 	2 	0.053913 	20 	3.779165e+07 	2 	0.280013 	0.0 	... 	POINT (-26.32802 72.88865) 	1.183334e+09 	1495.614258 	0.709607 	1494.015764 	1494.007690 	1494.036499 	81 	0.834192 	1492.483643
2019-03-14 09:40:48.354683848 	405839 	1160 	15 	2 	0.114113 	20 	3.779165e+07 	2 	0.330651 	0.0 	... 	POINT (-26.32806 72.88874) 	1.183334e+09 	1494.552612 	0.687070 	1492.995177 	1492.975098 	1492.975098 	81 	0.806113 	1491.506592
2019-03-14 09:40:48.356088496 	405839 	1160 	8 	2 	0.046961 	20 	3.779165e+07 	2 	0.132604 	0.0 	... 	POINT (-26.32810 72.88882) 	1.183334e+09 	1493.258545 	0.591286 	1491.916643 	1491.894531 	1491.894531 	81 	0.698308 	1490.576050
2019-03-14 09:40:48.357494344 	405840 	1160 	16 	2 	0.032820 	20 	3.779165e+07 	2 	0.124317 	0.0 	... 	POINT (-26.32814 72.88891) 	1.183334e+09 	1492.435181 	0.542995 	1491.242427 	1491.285156 	1491.201050 	81 	0.642991 	1489.934570
2019-03-14 09:40:48.358898136 	405840 	1160 	19 	2 	0.028111 	20 	3.779165e+07 	2 	0.117033 	0.0 	... 	POINT (-26.32818 72.88900) 	1.183334e+09 	1492.308105 	0.614543 	1490.753858 	1490.765381 	1490.621094 	81 	0.731349 	1489.323242

1650 rows × 25 columns
```
For a full description of all of the fields returned from the `atl06p` function, see the [elevations](../../user_guide/icesat2.html#elevations) documentation.

