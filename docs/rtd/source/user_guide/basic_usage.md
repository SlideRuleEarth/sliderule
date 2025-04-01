# Basic Usage

## Parameters

Parameters are passed to the SlideRule endpoints as JSON data structures (or as dictionaries to the Python client).  The parameters supported by Sliderule can be loosely divided into different groups based on the functionality they control.  The following parameter groups are generic parameters supported by most SlideRule services.  For parameters specific to a given SlideRule plugin, please refer to the plugin's documentation.

Polygons
--------------

All polygons provided to SlideRule must be provided as a list of dictionaries containing longitudes and latitudes in counter-clockwise order with the first and last point matching.

* ``"poly"``: polygon of region of interest
* ``"proj"``: projection used when subsetting data ("north_polar", "south_polar", "plate_carree"). In most cases, do not specify and code will do the right thing.
* ``"ignore_poly_for_cmr"``: boolean for whether to use the polygon as a part of the request to CMR for obtaining the list of resources to process. By default the polygon is used and this is only here for unusual cases where SlideRule is able to handle a polygon for subsetting that CMR cannot, and the list of resources to process is obtained some other way.

For example:

.. code-block:: python

    region = [ {"lon": -108.3435200747503, "lat": 38.89102961045247},
               {"lon": -107.7677425431139, "lat": 38.90611184543033},
               {"lon": -107.7818591266989, "lat": 39.26613714985466},
               {"lon": -108.3605610678553, "lat": 39.25086131372244},
               {"lon": -108.3435200747503, "lat": 38.89102961045247} ]
    parms = {
        "poly": region['poly']
    }

In order to facilitate other formats, the ``sliderule.toregion`` function can be used to convert polygons from the GeoJSON and Shapefile formats into this format accepted by `SlideRule`.

There is no limit to the number of points in the polygon, but note that as the number of points grow, the amount of time it takes to perform the subsetting process also grows. For that reason, it is recommended that if your polygon has more than a few hundred points, it is best to enable the rasterization option described in the GeoJSON section below.

GeoJSON
-------------

One of the outputs of the ``sliderule.toregion`` function is a GeoJSON object that describes the region of interest.  It is available under the ``"region_mask"`` element of the returned dictionary.

* ``"raster"``: geojson describing region of interest, enables use of rasterized region for subsetting

When supplied in the parameters sent in the request, the server side software forgoes using the polygon for subsetting operations, and instead builds a raster of the GeoJSON object using the specified cellsize, and then uses that raster image as a mask to determine which points in the source datasets are included in the region of interest.

For regions of interest that are complex and include many holes where a single track may have multiple intesecting and non-intersecting segments, the rasterized subsetting function is much more performant at the cost of the resolution of the subsetting operation.

The example code below shows how this option can be enabled and used (note, the ``"poly"`` parameter is still required):

.. code-block:: python

    region = sliderule.toregion('examples/grandmesa.geojson', cellsize=0.02)
    parms = {
        "poly": region['poly'],
        "region_mask": region['raster']
    }

Time
----------

All times sent as request parameters are in GMT time.  All times returned in result records are in number of seconds (fractual, double precision) since the GPS epoch which is January 6, 1980 at midnight (1980-01-06:T00.00.00.000000Z).

* ``"t0"``: start time for filtering source datasets (format %Y-%m-%dT%H:%M:%SZ, e.g. 2018-10-13T00:00:00Z)
* ``"t1"``: stop time for filtering source datasets (format %Y-%m-%dT%H:%M:%SZ, e.g. 2018-10-13T00:00:00Z)

The SlideRule Python client provides helper functions to perform the conversion.  See `gps2utc </web/rtd/api_reference/sliderule.html#gps2utc>`_.

For APIs that return GeoDataFrames, the **"time"** column values are represented as a ``datatime`` with microsecond precision.


Timeouts
----------------------

Each request supports setting three different timeouts. These timeouts should only need to be set by a user manually either when making extremely large processing requests, or when failing fast is necessary and default timeouts are too long.

* ``"rqst_timeout"``: total time in seconds for request to be processed
* ``"node_timeout"``: time in seconds for a single node to work on a distributed request (used for proxied requests)
* ``"read_timeout"``: time in seconds for a single read of an asset to take
* ``"timeout"``: global timeout setting that sets all timeouts at once (can be overridden by further specifying the other timeouts)

Raster Sampling
--------------------------------

SlideRule supports sampling raster datasets at the latitude and longitude of each calculated result from SlideRule processing.  When raster sampling is requested, the DataFrame returned by SlideRule includes columns for each requested raster with their associated values.

To request raster sampling, the ``"samples"`` parameter must be populated as a dictionary in the request.  Each key in the dictionary is used to label the data returned for that raster in the returned DataFrame.

* ``"samples"``: dictionary of rasters to sample
    - ``"<key>"``: user supplied name used to identify results returned from sampling this raster
        - ``"asset"``: name of the raster (as supplied in the Asset Directory) to be sampled
        - ``"algorithm"``: algorithm to use to sample the raster; the available algorithms for sampling rasters are: NearestNeighbour, Bilinear, Cubic, CubicSpline, Lanczos, Average, Mode, Gauss
        - ``"radius"``: the size of the kernel in meters when sampling a raster; the size of the region in meters for zonal statistics
        - ``"zonal_stats"``: boolean whether to calculate and return zonal statistics for the region around the location being sampled
        - ``"with_flags"``: boolean whether to include auxiliary information about the sampled pixel in the form of a 32-bit flag
        - ``"t0"``: start time for filtering rasters to be sampled (format %Y-%m-%dT%H:%M:%SZ, e.g. 2018-10-13T00:00:00Z)
        - ``"t1"``: stop time for filtering rasters to be sampled (format %Y-%m-%dT%H:%M:%SZ, e.g. 2018-10-13T00:00:00Z)
        - ``"substr"``: substring filter for rasters to be sampled; the raster will only be sampled if the name of the raster includes the provided substring (useful for datasets that have multiple rasters for a given geolocation to be sampled)
        - ``"closest_time"``: time used to filter rasters to be sampled; only the raster that is closest in time to the provided time will be sampled - can be multiple rasters if they all share the same time (format %Y-%m-%dT%H:%M:%SZ, e.g. 2018-10-13T00:00:00Z)
        - ``"use_poi_time"``: overrides the "closest_time" setting (or provides one if not set) with the time associated with the point of interest being sampled
        - ``"catalog"``: geojson formatted stac query response (obtained through the `sliderule.earthdata.stac` Python API)
        - ``"bands"``: list of bands to read out of the raster, or a predefined algorithm that combines bands for a given dataset
        - ``"key_space"``: 64-bit integer defining the upper 32-bits of the ``file_id``; this in general should never be set as the server will typically do the right thing assigning a key space;   but for users that are parallelizing requests on the client-side, this parameter can be usedful when constructing the resulting file dictionaries that come back with the raster samples

.. code-block:: python

    parms {
        "samples" : {
            "mosaic": {"asset": "arcticdem-mosaic", "radius": 10.0, "zonal_stats": True},
            "strips": {"asset": "arcticdem-strips", "algorithm": "CubicSpline"}
        }
    }

The output returned in the DataFrame can take two different forms depending on the nature of the data requested.

(1) If the raster being sampled includes on a single value for each latitude and longitude, then the data returned will be of the form {key}.value, {key}.time, {key}.file_id, {key}.{zonal stat} where the zonal stats are only present if requested.

(2) If the raster being sampled has multiple values for a given latitude and longitude (e.g. multiple strips per scene, or multiple bands per image), then the data returned will still have the same column headers, but the values will be numpy arrays.  For a given row in a DataFrame, the length of the numpy array in each column associated with a raster should be the same.  From row to row, those lengths can be different.


Output parameters
------------------------

By default, SlideRule returns all results in a native (i.e. custom to SlideRule) format that is streamed back to the client and used by the client to construct a GeoDataFrame that is returned to the user. Using the parameters below, SlideRule can build the entire DataFrame of the results on the servers, in one of a few different formats (currently, only GeoParquet `GeoParquet <./GeoParquet.html>`_ is supported), and return the results as a file that is streamed back to the client and reconstructed by the client.  To control this behavior, the ``"output"`` parameter is used.

Optionally, SlideRule supports writing the output to an S3 bucket instead of streaming the output back to the client.  In order to enable this behavior, the ``"output.path"`` field must start with "s3://" followed by the bucket name and object key.  For example, if you wanted the result to be written to a file named "grandmesa.parquet" in your S3 bucket "mybucket", in the subfolder "maps", then the output.path would be "s3://mybucket/maps/grandmesa.parquet".  When writing to S3, it is required by the user to supply the necessary credentials.  This can be done in one of two ways: (1) the user specifies an "asset" supported by SlideRule for which SlideRule already maintains credentials; (2) the user specifies their own set of temporary aws credentials.

* ``"output"``: settings to control how SlideRule outputs results
    * ``"path"``: the full path and filename of the file to be constructed by the client, ``NOTE`` - the path MUST BE less than 128 characters
    * ``"format"``: the format of the file constructed by the servers and sent to the client (currently, only GeoParquet is supported, specified as "parquet")
    * ``"open_on_complete"``: boolean; if true then the client is to open the file as a DataFrame once it is finished receiving it and writing it out; if false then the client returns the name of the file that was written
    * ``"region"``: AWS region when the output path is an S3 bucket (e.g. "us-west-2")
    * ``"asset"``: the name of the SlideRule asset from which to get credentials for the optionally supplied S3 bucket specified in the output path
    * ``"credentials"``: the AWS credentials for the optionally supplied S3 bucket specified in the output path
      - ``"aws_access_key_id"``: AWS access key id
      - ``"aws_secret_access_key"``: AWS secret access key
      - ``"aws_session_token"``: AWS session token

.. code-block:: python

    parms {
        "output": { "path": "grandmesa.parquet", "format": "parquet", "open_on_complete": True }
    }

Parameter reference table
------------------------------

The default set of parameters used by SlideRule are set to match anticipated user needs and should be good to use for most requests.

.. list-table:: SlideRule Request Parameters
   :widths: 25 25 50
   :header-rows: 1

   * - Parameter
     - Units
     - Default
   * - ``"closest_time"``
     - String, YYYY-MM-DDThh:mm:ss
     -
   * - ``"key_space"``
     - Integer
     -
   * - ``"node_timeout"``
     - Integer, seconds
     - 600
   * - ``"output.asset"``
     - String
     -
   * - ``"output.credentials.aws_access_key_id"``
     - String
     -
   * - ``"output.credentials.aws_secret_access_key"``
     - String
     -
   * - ``"output.credentials.aws_session_token"``
     - String
     -
   * - ``"output.format"``
     - String
     -
   * - ``"output.open_on_complete"``
     - Boolean
     - False
   * - ``"output.path"``
     - String, file path
     -
   * - ``"poly"``
     - String, JSON
     -
   * - ``"proj"``
     - String
     -
   * - ``"ignore_poly_for_cmr"``
     - Boolean
     - False
   * - ``"region_mask"``
     - String, JSON
     -
   * - ``"read_timeout"``
     - Integer, seconds
     - 600
   * - ``"region"``
     - Integer, orbit region
     -
   * - ``"rqst_timeout"``
     - Integer, seconds
     - 600
   * - ``"samples.asset"``
     - String, raster asset name
     -
   * - ``"samples.algorithm"``
     - String, algorithm name
     - "NearestNeighbour"
   * - ``"samples.radius"``
     - Float, meters
     - 0
   * - ``"samples.zonal_stats"``
     - Boolean
     - False
   * - ``"substr"``
     - String
     -
   * - ``"timeout"``
     - Integer, seconds
     -
   * - ``"t0"``
     - String, YYYY-MM-DDThh:mm:ss
     -
   * - ``"t1"``
     - String, YYYY-MM-DDThh:mm:ss
     -
   * - ``"with_flags"``
     - Boolean
     - False
