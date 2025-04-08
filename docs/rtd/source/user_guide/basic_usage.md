# Basic Usage

## Using SlideRule

There are four steps to using SlideRule:
1. Import the client package
2. Optionally configure the client
3. Define the request parameters
4. Issue the processing request

### Import the client package

The majority of the SlideRule Python client functionality is found in the `sliderule` module; but there are other modules as well that include additional features and mission specific functions and variables.  To import the client and start using Sliderule, you can use the following code:
```python
from sliderule import sliderule
```

Here is a list of modules in the SlideRule Python client.

sliderule
  Core functionality: initialization and configuration, making requests, processing an area of interest.

earthdata
  Query for resources using CMR, CMR-STAC, TNM, and other services.

h5
  Directly read HDF5 data from the cloud using the server-side H5Coro implementation.

raster
  Sample and subset supported raster datasets

icesat2
  Issue processing requests for ICESat-2 standard and custom data products

gedi
  Issue processing requests for GEDI standard and custom data products

io
  Read and write SlideRule output in different formats

ipysliderule
  Widgets and routines for building interactive interfaces to SlideRule in a Jupyter notebook

If you wanted to use multiple modules from the SlideRule Python client, you could use the following code as an example:
```python
from sliderule import sliderule, earthdata, icesat2
```

### Optionally Configure the Client

You can begin using the SlideRule Python client right away without any configuration.  By default, the client is configured to use the public cluster and issue a minimal number of log messages, and if no further configuration is performed, all future processing requests will work with those settings.

But the client provides numerous routines for configuring its behavior.  Here is a list and short description of each of the available routines.  See the API Reference for a more detailed description.

sliderule.init
  Primary routine for configuring client; attempts to capture as parameters the typical settings that a user would want to change.

sliderule.set_url
  Configure the domain that the client points to for the server-side cluster; defaults to `slideruleearth.io` but can be changed to things like `localhost` for local development.  Typically, this should not be changed.

sliderule.set_ssl_verify
  Disable the SSL certification check when making processing requests to sliderule; by default the client verifies the cert, but in cases (usually development) when a cert is invalid but the user knows the server being pointed to is valid, this setting can be overridden to allow the requests to go through.  Typically, this should not be changed.

sliderule.set_verbose
  Change the verbosity of the log messages being generated; when enabled, server-side log messages will be printed to the user console.

sliderule.set_rqst_timeout
  Change how long to wait for the request to finish; needed when a user is making a very large processing request and needs to match the client timeouts to the server-side timeouts provided in the request parameters.

sliderule.set_processing_flags
  Certain streamed responses flag auxillary fields in their response structure that indicate those fields are not necessary for core functionality; the client can be configured to skip those fields in order to speed up processing large responses.

sliderule.update_available_servers
  Acquire the number of nodes in the cluster and make a request to change (e.g. request a capacity increase) the number of nodes in a cluster.

sliderule.scaleout
  Increase the number of nodes in a cluster and wait for the cluster to reach the requested capacity

sliderule.authenticate
  Configure the organization that the client points to for processing requests.  This is paired with the `sliderule.set_url()` routine to create the full URL for processing requests.  For example, if the user calls `sliderule.set_url("slideruleearth.io")` and then `sliderule.authenticate("uw")` then the client will make all requests to `https://uw.slideruleearth.io`.  This routine also handles authenticating to the organization when the associated cluster is a private cluster.

### Define the Request Parameters

When making a request to the SlideRule servers, the parameters of the request (i.e. what the user wants to process and how they want to process it) are supplied in the body of the request as a JSON structure.  When using the SlideRule Python client, the parameters are captured and provided by the user in a Python dictionary, and the dictionary is automatically serialized into a JSON structure by the client when making the request.

For example, to set the __confidence__ filter on an ATL03 subsetting request, the parameter structure needed by the endpoint can be passed into the `sliderule.run()` function as a dictionary, like so:
```python
sliderule.run("atl03x", {"cnf":-1}, resources=["ATL03_20181019065445_03150111_005_01.h5"])
```

### Issue the Processing Request

There are two general purpose routines provided in the SlideRule Python client for issuing processing requests.

sliderule.source
  Implements the low-level protocol for making requests to SlideRule and processing the results.  This can be used to issue a request to any SlideRule endpoint.

sliderule.run
  Implements a standard SlideRule convention for making requests to SlideRule endpoints that return a dataframe.  This uses the `sliderule.source()` routine.

A user is always free to use one of the routines above for making requests to SlideRule, but many times it is more convenient to use one of the helper functions in the mission specific modules.  For instance, when making processing requests for ICESat-2 data, the `icesat2` module provides many routines that wrap calls to specific enpoints in an easy-to-use Python function.  For instance, when making a request to the `atl06p` endpoint, a user should use the `icesat2.atl06p()` Python routine.

## General Request Parameters

### Resources

All requests must provide some way for the server side code to know which resources to process.  Typically, that is accomplished via specifying an area of interest or other query parameters; but sometimes it is necessary to manually specify which resources to process.  When that is the case, there are a few parameters the user can use to do so:
* `resources`: a list of resources to process (e.g. granule names like "ATL03_20181019065445_03150111_005_01.h5")
* `asset`: the name of a collection of resources; this rarely needs to be specified because the default value for most endpoints are sufficient

### Polygons

All polygons provided to SlideRule must be provided as a list of dictionaries containing longitudes and latitudes in counter-clockwise order with the first and last point matching.

The applicable parameters used to specify the polygon are:
* `poly`: polygon of region of interest
* `proj`: projection used when subsetting data ("north_polar", "south_polar", "plate_carree"). In most cases, do not specify and code will do the right thing.
* `ignore_poly_for_cmr`: boolean for whether to use the polygon as a part of the request to CMR for obtaining the list of resources to process. By default the polygon is used and this is only here for unusual cases where SlideRule is able to handle a polygon for subsetting that CMR cannot, and the list of resources to process is obtained some other way.

For example:

```python

    region = [ {"lon": -108.3435200747503, "lat": 38.89102961045247},
               {"lon": -107.7677425431139, "lat": 38.90611184543033},
               {"lon": -107.7818591266989, "lat": 39.26613714985466},
               {"lon": -108.3605610678553, "lat": 39.25086131372244},
               {"lon": -108.3435200747503, "lat": 38.89102961045247} ]
    parms = {
        "poly": region['poly']
    }
```

In order to facilitate other formats, the `sliderule.toregion` function can be used to convert polygons from the GeoJSON and Shapefile formats into this format accepted by `SlideRule`.

#### Rasterized Area of Interest

There is no limit to the number of points in the polygon, but note that as the number of points grow, the amount of time it takes to perform the subsetting process also grows. Also, some regions cannot be expressed as a single polygon because they have holes in them or define discrete unconnected areas. Because of this, one of the outputs of the `sliderule.toregion` function is a GeoJSON object for describing complex geometries.  It is available under the `"region_mask"` element of the returned dictionary.

When the GeoJSON is supplied in the parameters sent in the request, the server side software forgoes using the polygon for subsetting operations, and instead builds a raster of the GeoJSON object using the specified cellsize, and then uses that raster image as a mask to determine which points in the source datasets are included in the region of interest.  The applicable parameters use to specify this functionality are:
* `raster`: geojson describing region of interest, enables use of rasterized region for subsetting
* `cellsize`: the resolution to rasterize the GeoJSON; currently the units are spherical degrees and no projections are supported, but support for projections will be included in future releases

The example code below shows how this option can be enabled and used (note, the `poly` parameter is still required):

```python

    region = sliderule.toregion('examples/grandmesa.geojson', cellsize=0.02)
    parms = {
        "poly": region['poly'],
        "region_mask": region['raster']
    }
```

### Timeouts

Each request supports setting three different timeouts. These timeouts should only need to be set by a user manually either when making extremely large processing requests, or when failing fast is necessary and default timeouts are too long.

* `rqst_timeout`: total time in seconds for request to be processed
* `node_timeout`: time in seconds for a single node to work on a distributed request (used for proxied requests)
* `read_timeout`: time in seconds for a single read of an asset to take
* `timeout`: global timeout setting that sets all timeouts at once (can be overridden by further specifying the other timeouts)

### Time

A time range is typically used to limit the resources being processed to only include those resources collected within the time range specified.  All times sent as request parameters are in GMT time.  All times returned in result records are in number of seconds (fractual, double precision) since the GPS epoch which is January 6, 1980 at midnight (1980-01-06:T00.00.00.000000Z).

* `t0`: start time for filtering source datasets (format %Y-%m-%dT%H:%M:%SZ, e.g. 2018-10-13T00:00:00Z)
* `t1`: stop time for filtering source datasets (format %Y-%m-%dT%H:%M:%SZ, e.g. 2018-10-13T00:00:00Z)

The SlideRule Python client provides helper functions to perform the conversion.  See `gps2utc </web/rtd/api_reference/sliderule.html#gps2utc>`_.

For APIs that return GeoDataFrames, the columns that hold times are represented as a ``datatime`` with microsecond precision.  In most cases, the applicable time column will be used as the index of the GeoDataFrame.
