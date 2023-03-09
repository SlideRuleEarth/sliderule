# Returning Data from SlideRule in the GeoParquet Format

2022-12-23


## Overview

This tutorial walks you through the steps necessary to return data from SlideRule in the GeoParquet format.

**Prerequisites**: This walk-through assumes you have already installed the SlideRule Python client and familiar with how to use it. See the [installation](../../getting_started/Install.html) instructions in the reference documentation if you need help installing the SlideRule Python client.  See the [Making Your First Request](./first_request.html) tutorial if you've never made a SlideRule processing request before.


## Background

GeoParquet is a cloud-optimized format for storing geospatial datasets.  It is built on top of Apache's Parquet format and is fully compatible with all Parquet-based tools.  The official specification for GeoParquet can be found here: https://github.com/opengeospatial/geoparquet.

By default, SlideRule uses its own native streaming format for de/serializing data across a network.  As processing results are produced by SlideRule, they are immediately transmitted over the network to the requestor.  When the SlideRule Python client is being used, these results are used to construct a GeoDataFrame on the fly.  While this approach as some advantages - namely low latency responses and low memory usage on the server - it also has some drawbacks.  Chief among them is the processing required to construct the DataFrame.  Data returned by the SlideRule service can be thought of as rows of information.  As each row is received it is appended to the final DataFrame.  But because DataFrames are columnar-based, each time data is appended, costly memory allocations and data copies result.  Effort has been made to optimize this processing in the client, but ultimately only so much can be done and the problem remains that it is encumbent on the client to parse, rearrange, and construct the DataFrame.

For small responses (less than 1M points), things are okay.  But as responses get larger, the client is unable to keep up with the SlideRule servers, and can bottleneck the process or even crash if it runs out of memory.  To address these shortcomings, SlideRule supports sending responses back as GeoParquet files.  When a GeoParquet file is requested, the results of the request are built entirely on the servers as a GeoParquet file, and then the final file is streamed back to the client where it is directly written to disk.  This allows large requests to consume server-side resources in parsing, rearranging, and building a DataFrame-like structure.  Clients can then choose to open the resulting GeoParquet file immediately, or open it at some later time with different software.


## Requesting the GeoParquet Format

The __"output"__ parameter is used to request the GeoParquet format.


__Step 1__: Import and initialize the SlideRule Python package for ICESat-2.
```python
>>> from sliderule import sliderule, icesat2
>>> icesat2.init("slideruleearth.io")
```

__Step 2__: Create parameters for a typical `atl06p` processing request (it could also be `atl03sp`).
```python
>>> grand_mesa = sliderule.toregion('grandmesa.geojson')
>>> parms = {
    "poly": grand_mesa["poly"],
    "srt": icesat2.SRT_LAND,
    "cnf": icesat2.CNF_SURFACE_HIGH,
    "len": 40.0,
    "res": 20.0,
    "maxi": 6
}
```
The **grandmesa.geojson** file used in this example can be downloaded by navigating to our [downloads](/rtd/tutorials/downloads.html) page; alternatively, you can create your own GeoJSON file at [geojson.io](https://geojson.io).

__Step 3__: Specify the GeoParquet format.
```python
>>> parms["output"] = { "path": "grandmesa.parquet", "format": "parquet", "open_on_complete": True }
```

The _"path"_ parameter is the name of the local file the client will write the parquet output to.

The _"format"_ parameter is what specifies that the GeoParquet format is requested.

The *"open_on_complete"* means that the client will return a GeoDataFrame of the opened GeoParquet file when making a call to the `atl06p` or `atl03sp` functions.  If this option is false, then the client returns the name of the file.

__Step 4__: Issue the processing request to SlideRule.
```python
>>> gdf = icesat2.atl06p(parms)
```
When this completes (~45 seconds), the _gdf_ variable should now contain all of the results of the elevations calculated by SlideRule; and there should be a grandmesa.parquet file in the directory where Python was run from.

__Step 5__: Display a summary of the results.
```python
>>> print(gdf)

	extent_id 	distance 	segment_id 	rgt 	rms_misfit 	delta_time 	gt 	dh_fit_dy 	n_fit_photons 	lat 	h_sigma 	lon 	pflags 	spot 	h_mean 	cycle 	w_surface_window_final 	dh_fit_dx 	geometry
0 	3319153005902693763 	4.331749e+06 	216001 	737 	0.328377 	2.755125e+07 	60 	0.0 	31 	38.935416 	0.117401 	-108.032116 	0 	1 	2103.522185 	1 	3.000000 	0.112198 	POINT (38.93542 -108.03212)
1 	3319153005902693767 	4.331769e+06 	216002 	737 	0.347173 	2.755125e+07 	60 	0.0 	30 	38.935596 	0.088707 	-108.032137 	0 	1 	2105.439653 	1 	3.000000 	0.097912 	POINT (38.93560 -108.03214)
2 	3319153005902693771 	4.331790e+06 	216003 	737 	0.406988 	2.755125e+07 	60 	0.0 	10 	38.935776 	0.143279 	-108.032158 	0 	1 	2106.078998 	1 	4.484487 	-0.003341 	POINT (38.93578 -108.03216)
3 	3319153005902693775 	4.331810e+06 	216004 	737 	0.297643 	2.755125e+07 	60 	0.0 	40 	38.935956 	0.085242 	-108.032179 	0 	1 	2106.949294 	1 	3.000000 	0.106623 	POINT (38.93596 -108.03218)
4 	3319153005902693779 	4.331830e+06 	216005 	737 	0.280220 	2.755125e+07 	60 	0.0 	73 	38.936135 	0.032952 	-108.032201 	0 	1 	2109.070343 	1 	3.000000 	0.100320 	POINT (38.93614 -108.03220)
... 	... 	... 	... 	... 	... 	... 	... 	... 	... 	... 	... 	... 	... 	... 	... 	... 	... 	... 	...
262158 	1328563070116566907 	4.354303e+06 	217125 	295 	2.831141 	1.505993e+08 	60 	0.0 	184 	39.139200 	0.208896 	-108.295364 	0 	1 	1608.092662 	17 	16.216058 	-0.506227 	POINT (39.13920 -108.29536)
262159 	1328563070116566915 	4.354343e+06 	217127 	295 	3.100837 	1.505993e+08 	60 	0.0 	166 	39.139559 	0.242325 	-108.295405 	0 	1 	1596.932221 	17 	17.165378 	-0.239357 	POINT (39.13956 -108.29541)
262160 	1328563070116566911 	4.354323e+06 	217126 	295 	2.389764 	1.505993e+08 	60 	0.0 	169 	39.139380 	0.183829 	-108.295384 	0 	1 	1600.750765 	17 	13.859626 	-0.156372 	POINT (39.13938 -108.29538)
262161 	1328563070116566919 	4.354363e+06 	217128 	295 	3.053285 	1.505993e+08 	60 	0.0 	166 	39.139739 	0.236985 	-108.295426 	0 	1 	1591.588886 	17 	17.106291 	-0.291532 	POINT (39.13974 -108.29543)
262162 	1328563070116566923 	4.354383e+06 	217129 	295 	2.411602 	1.505993e+08 	60 	0.0 	176 	39.139919 	0.181812 	-108.295448 	0 	1 	1586.793077 	17 	12.933652 	-0.179786 	POINT (39.13992 -108.29545)

262163 rows × 19 columns
```
For a full description of all of the fields returned from the `atl06p` function, see the [elevations](../../user_guide/ICESat-2.html#elevations) documentation.

