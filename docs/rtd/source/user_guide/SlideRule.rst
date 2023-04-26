=====================
SlideRule Core Module
=====================

1. Service Architecture
#######################

SlideRule is a **real-time** science data processing service; when a user makes a request to SlideRule, the response is returned right away.  In technical terms, this means that the HTTP response data is sent back over the same TCP/IP connection as was used for the HTTP request.  This is in contrast to a job based processing service where job requests are issued by users, and then at some later time the user is notified that their data is ready and available for download.

While SlideRule supports large processing requests that may take minutes (or even hours), the same paradigm holds.  Client software makes requests to SlideRule, SlideRule immediately begins processing the request and generating response data, and that data is streamed back to the user while the client software maintains an active connection with the SlideRule server.

2. Python Client
################

Using SlideRule means issuing HTTP requests to SlideRule's endpoints to perform processing requests and get back science data results.  But in practice, a typical workflow involves much more than issuing requests.  A typical workflow involves:

#. Defining an *area of interest* as a geographic region over a period of time
#. Determining *which data* is available in that area of interest
#. Describing *what processing* you want to perform
#. Issuing the processing *request*
#. Parsing the *response* data into a useable data structure

The SlideRule Python Client helps make the above steps a lot easier by providing a user-friendly interface for accomplishing typical workflows.

#. The Python client supports an *area of interest* being defined in mulitple ways - as a dictionary of latitude and longitudes, a list of coordinates, a GeoJSON file, a Shapefile, or a GeoDataFrame - and then representing the area of interest in standard formats accepted by SlideRule and other NASA web services.
#. The Python client queries NASA's Common Metadata Repository (CMR) system automatically for the user and populates the requests to SlideRule with the available science data pertinent to the user's request.
#. The Python client allows users to define their processing parameters as Python dictionaries, and make requests to SlideRule using Python functions.
#. The Python client handles the HTTPS connection to the SlideRule servers as well as any necessary authentation requests to the SlideRule Provisioning System when private clusters are being used.
#. The Python client parses the response data from SlideRule and presents a standard GeoDataFrame result back to the user.

3. De-serialization
###################

There are two types of SlideRule services distinguished by the type of response they provide: (1) **normal** services, (2) **stream** services.

Normal
    Normal services are accessed via the `GET` HTTP method and return a discrete block of ASCII text, typically formatted as JSON.

    These services can easily be accessed via `curl` or other HTTP-based tools, and contain self-describing data.
    When using the SlideRule Python client, they are accessed via the ``sliderule.source(..., stream=False)`` call.

Stream
    Stream services are accessed via the `POST` HTTP method and return a serialized stream of binary records containing the results of the processing request.

    These services are more difficult to work with third-party tools since the returned data must be parsed and the data itself is not self-describing.
    When using the SlideRule Python client, they are accessed via the ``sliderule.source(..., stream=True)`` call.  Inside this call, the SlideRule Python client
    takes care of any additional service calls needed in order to parse the results and return a self-describing Python data structure (i.e. the elements of
    the data structure are named in such a way as to indicate the type and content of the returned data).

    If you want to process streamed results outside of the SlideRule Python client, then a brief description of the format of the data follows.
    For additional guidance, the hidden functions inside the *sliderule.py* source code provide the code which performs
    the stream processing for the SlideRule Python client.

    Each response record is formatted as: `<record header><record type><record data>` where,

    record header
        64-bit big endian structure providing the version and length of the record: `<version:16 bits><type size:16 bits><data size:32 bits>`.
    record type
        null-terminated ASCII string containing the name of the record type
    record data
        binary contents of data

    In order to know how to process the contents of the **record data**, the user must perform an additional query to the SlideRule ``definition`` service,
    providing the **record type**.  The **definition** service returns a JSON response object that provides a format definition of the record type that can
    be used by the client to decode the binary record data.  The format of the **definition** response object is:

    .. code-block:: python

        {
            "__datasize": # minimum size of record
            "field_1":
            {
                "type": # data type (see sliderule.basictypes for full definition), or record type if a nested structure
                "elements": # number of elements, 1 if not an array
                "offset": # starting bit offset into record data
                "flags": # processing flags - LE: little endian, BE: big endian, PTR: pointer
            },
            ...
            "field_n":
            {
                ...
            }
        }

4. Assets
#########

When accessing SlideRule as a service, there are times when you need to specify which source datasets it should use when processing the data.
A source dataset is called an **asset** and is specified by its name as a string.

The asset name tells SlideRule where to get the data, and what format the data should be in. The following assets are supported by the current deployment of SlideRule:

.. csv-table::
    :header: asset,             identity,       driver,     path,                                                                   index,              region,     endpoint
    icesat2,                    nsidc-cloud,    cumulus,    nsidc-cumulus-prod-protected,                                           nil,                us-west-2,  https://s3.us-west-2.amazonaws.com
    gedil4a,                    ornl-cloud,     s3,         ornl-cumulus-prod-protected/gedi/GEDI_L4A_AGB_Density_V2_1/data,        nil,                us-west-2,  https://s3.us-west-2.amazonaws.com
    gedil4b,                    ornl-cloud,     s3,         /vsis3/ornl-cumulus-prod-protected/gedi/GEDI_L4B_Gridded_Biomass/data,         GEDI04_B_MW019MW138_02_002_05_R01000M_V2.tif,              us-west-2, https://s3.us-west-2.amazonaws.com
    gedil3-elevation,           ornl-cloud,     s3,         /vsis3/ornl-cumulus-prod-protected/gedi/GEDI_L3_LandSurface_Metrics_V2/data,   GEDI03_elev_lowestmode_mean_2019108_2022019_002_03.tif,    us-west-2, https://s3.us-west-2.amazonaws.com
    gedil3-canopy,              ornl-cloud,     s3,         /vsis3/ornl-cumulus-prod-protected/gedi/GEDI_L3_LandSurface_Metrics_V2/data,   GEDI03_rh100_mean_2019108_2022019_002_03.tif,              us-west-2, https://s3.us-west-2.amazonaws.com
    gedil3-elevation-stddev,    ornl-cloud,     s3,         /vsis3/ornl-cumulus-prod-protected/gedi/GEDI_L3_LandSurface_Metrics_V2/data,   GEDI03_elev_lowestmode_stddev_2019108_2022019_002_03.tif,  us-west-2, https://s3.us-west-2.amazonaws.com
    gedil3-canopy-stddev,       ornl-cloud,     s3,         /vsis3/ornl-cumulus-prod-protected/gedi/GEDI_L3_LandSurface_Metrics_V2/data,   GEDI03_rh100_stddev_2019108_2022019_002_03.tif,            us-west-2, https://s3.us-west-2.amazonaws.com
    gedil3-counts,              ornl-cloud,     s3,         /vsis3/ornl-cumulus-prod-protected/gedi/GEDI_L3_LandSurface_Metrics_V2/data,   GEDI03_counts_2019108_2022019_002_03.tif,                  us-west-2, https://s3.us-west-2.amazonaws.com
    gedil2a,                    iam-role,       s3,         sliderule/data/GEDI,                                                    nil,                us-west-2,  https://s3.us-west-2.amazonaws.com
    gedil1b,                    iam-role,       s3,         sliderule/data/GEDI,                                                    nil,                us-west-2,  https://s3.us-west-2.amazonaws.com
    landsat-hls,                lpdaac-cloud,   nil,        /vsis3/lp-prod-protected,                                               nil,                us-west-2,  https://s3.us-west-2.amazonaws.com
    arcticdem-mosaic,           nil,            nil,        /vsis3/pgc-opendata-dems/arcticdem/mosaics/v3.0/2m,                     2m_dem_tiles.vrt,   us-west-2,  https://s3.us-west-2.amazonaws.com
    arcticdem-strips,           nil,            nil,        /vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m,                    nil,                us-west-2,  https://s3.us-west-2.amazonaws.com
    rema-mosaic,                nil,            nil,        /vsis3/pgc-opendata-dems/rema/mosaics/v2.0/2m,                          2m_dem_tiles.vrt,   us-west-2,  https://s3.us-west-2.amazonaws.com
    rema-strips,                nil,            nil,        /vsis3/pgc-opendata-dems/rema/strips/s2s041/2m,                         nil,                us-west-2,  https://s3.us-west-2.amazonaws.com
    atlas-local,                local,          file,       /data/ATLAS,                                                            nil,                local,      local
    gedi-local,                 local,          file,       /data/GEDI,                                                             nil,                local,      local
    atlas-s3,                   iam-role,       s3,         sliderule/data/ATLAS,                                                   nil,                us-west-2,  https://s3.us-west-2.amazonaws.com
    nsidc-s3,                   nsidc-cloud,    cumulus,    nsidc-cumulus-prod-protected,                                           nil,                us-west-2,  https://s3.us-west-2.amazonaws.com

5. Parameters
#############

Parameters are passed to the SlideRule endpoints as JSON data structures (or as dictionaries to the Python client).  Not all parameters need to be defined when making a request - typically only a few parameters are used for any given request; there are reasonable defaults used for each parameter so that only those parameters that you want to customize need to be specified.  The parameters supported by Sliderule can be loosely divided into different groups based on the functionality they control.  The following parameter groups are generic parameters supported by most SlideRule services.  For parameters specific to a given SlideRule plugin, please refer to the plugin's documentation.

5.1 Polygons
--------------

All polygons provided to SlideRule must be provided as a list of dictionaries containing longitudes and latitudes in counter-clockwise order with the first and last point matching.

For example:

.. code-block:: python

    region = [ {"lon": -108.3435200747503, "lat": 38.89102961045247},
               {"lon": -107.7677425431139, "lat": 38.90611184543033},
               {"lon": -107.7818591266989, "lat": 39.26613714985466},
               {"lon": -108.3605610678553, "lat": 39.25086131372244},
               {"lon": -108.3435200747503, "lat": 38.89102961045247} ]

In order to facilitate other formats, the ``sliderule.toregion`` function can be used to convert polygons from the GeoJSON and Shapefile formats to the format accepted by `SlideRule`.

There is no limit to the number of points in the polygon, but note that as the number of points grow, the amount of time it takes to perform the subsetting process also grows. For that reason, it is recommended that if your polygon has more than a few hundred points, it is best to enable the rasterization option described in the GeoJSON section below.

5.2 GeoJSON
-------------

One of the outputs of the ``sliderule.toregion`` function is a GeoJSON object that describes the region of interest.  It is available under the ``"raster"`` element of the returned dictionary.

When supplied in the parameters sent in the request, the server side software forgoes using the polygon for subsetting operations, and instead builds a raster of the GeoJSON object using the specified cellsize, and then uses that raster image as a mask to determine which points in the source datasets are included in the region of interest.

For regions of interest that are complex and include many holes where a single track may have multiple intesecting and non-intersecting segments, the rasterized subsetting function is much more performant and the cost of the resolution of the subsetting operation.

The example code below shows how this option can be enabled and used (note, the ``"poly"`` parameter is still required):

.. code-block:: python

    region = sliderule.toregion('examples/grandmesa.geojson', cellsize=0.02)
    parms = {
        "poly": region['poly'],
        "raster": region['raster']
    }

5.3 Time
----------

All times sent as request parameters are in GMT time.  All times returned in result records are in number of seconds (fractual, double precision) since the GPS epoch which is January 6, 1980 at midnight (1980-01-06:T00.00.00.000000Z).

* ``"t0"``: start time for filtering source datasets (format %Y-%m-%dT%H:%M:%SZ, e.g. 2018-10-13T00:00:00Z)
* ``"t1"``: stop time for filtering source datasets (format %Y-%m-%dT%H:%M:%SZ, e.g. 2018-10-13T00:00:00Z)

The SlideRule Python client provides helper functions to perform the conversion.  See `gps2utc </web/rtd/api_reference/sliderule.html#gps2utc>`_.

For APIs that return GeoDataFrames, the **"time"** column values are represented as a ``datatime`` with microsecond precision.


5.4 Timeouts
----------------------

Each request supports setting three different timeouts. These timeouts should only need to be set by a user manually either when making extremely large processing requests, or when failing fast is necessary and default timeouts are too long.

* ``"rqst-timeout"``: total time in seconds for request to be processed
* ``"node-timeout"``: time in seconds for a single node to work on a distributed request (used for proxied requests)
* ``"read-timeout"``: time in seconds for a single read of an asset to take
* ``"timeout"``: global timeout setting that sets all timeouts at once (can be overridden by further specifying the other timeouts)

5.5 Raster Sampling
--------------------------------

SlideRule supports sampling raster datasets at the latitude and longitude of each calculated result from SlideRule processing.  When raster sampling is requested, the DataFrame returned by SlideRule includes columns for each requested raster with their associated values.

To request raster sampling, the ``"samples"`` parameter must be populated as a dictionary in the request.  Each key in the dictionary is used to label the data returned for that raster in the returned DataFrame.

* ``"samples"``: dictionary of rasters to sample
    - ``"<key>"``: user supplied name used to identify results returned from sampling this raster
        - ``"asset"``: name of the raster (as supplied in the Asset Directory) to be sampled
        - ``"algorithm"``: algorithm to use to sample the raster; the available algorithms for sampling rasters are: NearestNeighbour, Bilinear, Cubic, CubicSpline, Lanczos, Average, Mode, Gauss
        - ``"radius"``: the size of the kernel in meters when sampling a raster; the size of the region in meters for zonal statistics
        - ``"zonal_stats"``: boolean whether to calculate and return zonal statistics for the region around the location being sampled
        - ``"with_flags"``: boolean whether to include auxillary information about the sampled pixel in the form of a 32-bit flag
        - ``"t0"``: start time for filtering rasters to be sampled (format %Y-%m-%dT%H:%M:%SZ, e.g. 2018-10-13T00:00:00Z)
        - ``"t1"``: stop time for filtering rasters to be sampled (format %Y-%m-%dT%H:%M:%SZ, e.g. 2018-10-13T00:00:00Z)
        - ``"substr"``: substring filter for rasters to be sampled; the raster will only be sampled if the name of the raster includes the provided substring (useful for datasets that have multiple rasters for a given geolocation to be sampled)
        - ``"closest_time"``: time used to filter rasters to be sampled; only the raster that is closest in time to the provided time will be sampled - can be multiple rasters if they all share the same time (format %Y-%m-%dT%H:%M:%SZ, e.g. 2018-10-13T00:00:00Z)
        - ``"key_space"``: 64-bit integer defining the upper 32-bits of the ``file_id``; this in general should never be set as the server will typically do the right thing assigning a key space;   but for users that are parallelizing requests on the client-side, this parameter can be usedful when constructing the resulting file dictionaries that come back with the raster samples

.. code-block:: python

    parms {
        "samples" : {
            "mosaic": {"asset": "arcticdem-mosaic", "radius": 10.0, "zonal_stats": True},
            "strips": {"asset": "arcticdem-strips", "algorithm": "CubicSpline"}
        }
    }

The output returned in the DataFrame can take two differnt forms depending on the nature of the data requested.

(1) If the raster being sampled includes on a single value for each latitude and longitude, then the data returned will be of the form {key}.value, {key}.time, {key}.file_id, {key}.{zonal stat} where the zonal stats are only present if requested.

(2) If the raster being sampled has multiple values for a given latitude and longitude (e.g. multiple strips per scene, or multiple bands per image), then the data returned will still have the same column headers, but the values will be numpy arrays.  For a given row in a DataFrame, the length of the numpy array in each column associated with a raster should be the same.  From row to row, those lengths can be different.


5.6 Output parameters
------------------------

By default, SlideRule returns all results in a native (i.e. custom to SlideRule) format that is streamed back to the client and used by the client to construct a GeoDataFrame that is returned to the user. Using the parameters below, SlideRule can build the entire DataFrame of the results on the servers, in one of a few different formats (currently, only GeoParquet `GeoParquet <./GeoParquet.html>`_ is supported), and return the results as a file that is streamed back to the client and reconstructed by the client.  To control thie behavior, the ``"output"`` parameter is used.

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

5.7 Parameter reference table
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
   * - ``"node-timeout"``
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
   * - ``"raster"``
     - String, JSON
     -
   * - ``"read-timeout"``
     - Integer, seconds
     - 600
   * - ``"region"``
     - Integer, orbit region
     -
   * - ``"rqst-timeout"``
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


6. Endpoints
############

definition
----------

``GET /source/definition <request payload>``

    Gets the record definition of a record type; used to parse binary record data

**Request Payload** *(application/json)*

    .. list-table::
       :header-rows: 1

       * - parameter
         - description
         - default
       * - **record-type**
         - the name of the record type
         - *required*

    **HTTP Example**

    .. code-block:: http

        GET /source/definition HTTP/1.1
        Host: my-sliderule-server:9081
        Content-Length: 23


        {"rectype": "atl03rec"}

    **Python Example**

    .. code-block:: python

        # Request Record Definition
        rsps = sliderule.source("definition", {"rectype": "atl03rec"}, stream=False)

**Response Payload** *(application/json)*

    JSON object defining the on-the-wire binary format of the record data contained in the specified record type.

    See `De-serialization <./SlideRule.html#de-serialization>`_ for a description of how to use the record definitions.



event
-----

``POST /source/event <request payload>``

    Return event messages (logs, traces, and metrics) in real-time that have occurred during the time the request is active

**Request Payload** *(application/json)*

    .. list-table::
       :header-rows: 1

       * - parameter
         - description
         - default
       * - **type**
         - type of event message to monitor: "LOG", "TRACE", "METRIC"
         - "LOG"
       * - **level**
         - minimum event level to monitor: "DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"
         - "INFO"
       * - **format**
         - the format of the event message: "FMT_TEXT", "FMT_JSON"; empty for binary record representation
         - *optional*
       * - **duration**
         - seconds to hold connection open
         - 0

    **HTTP Example**

    .. code-block:: http

        POST /source/event HTTP/1.1
        Host: my-sliderule-server:9081
        Content-Length: 48

        {"type": "LOG", "level": "INFO", "duration": 30}

    **Python Example**

    .. code-block:: python

        # Build Logging Request
        rqst = {
            "type": "LOG",
            "level" : "INFO",
            "duration": 30
        }

        # Retrieve logs
        rsps = sliderule.source("event", rqst, stream=True)

**Response Payload** *(application/octet-stream)*

    Serialized stream of event records of the type ``eventrec``.  See `De-serialization <./SlideRule.html#de-serialization>`_ for a description of how to process binary response records.



geo
---

``GET /source/geo <request payload>``

    Perform geospatial operations on spherical and polar coordinates

**Request Payload** *(application/json)*

    .. list-table::
       :header-rows: 1

       * - parameter
         - description
         - default
       * - **asset**
         - data source (see `Assets <#assets>`_)
         - *required*
       * - **pole**
         - polar orientation of indexing operations: "north", "south"
         - "north"
       * - **lat**
         - spherical latitude coordinate to project onto a polar coordinate system, -90.0 to 90.0
         - *optional*
       * - **lon**
         - spherical longitude coordinate to project onto a polar coordinate system, -180.0 to 180.0
         - *optional*
       * - **x**
         - polar x coordinate to project onto a spherical coordinate system
         - *optional*
       * - **y**
         - polar y coordinate to project onto a spherical coordinate system
         - *optional*
       * - **span**
         - a box defined by a lower left latitude/longitude pair, and an upper right lattitude/longitude pair
         - *optional*
       * - **span1**
         - a span used for intersection with the span2
         - *optional*
       * - **span2**
         - a span used for intersection with the span1
         - *optional*

    .. list-table:: span definition
       :header-rows: 1

       * - parameter
         - description
         - default
       * - **lat0**
         - smallest latitude (starting at -90.0)
         - *required*
       * - **lon0**
         - smallest longitude (starting at -180.0)
         - *required*
       * - **lat1**
         - largest latitude (ending at 90.0)
         - *required*
       * - **lon1**
         - largest longitude (ending at 180.0)
         - *required*

    **HTTP Example**

    .. code-block:: http

        GET /source/geo HTTP/1.1
        Host: my-sliderule-server:9081
        Content-Length: 115


        {"asset": "atlas-local", "pole": "north", "lat": 30.0, "lon": 100.0, "x": -0.20051164424058, "y": -1.1371580426033}

    **Python Example**

    .. code-block:: python

        rqst = {
            "asset": "atlas-local",
            "pole": "north",
            "lat": 30.0,
            "lon": 100.0,
            "x": -0.20051164424058,
            "y": -1.1371580426033,
        }

        rsps = sliderule.source("geo", rqst)


**Response Payload** *(application/json)*

    JSON object with elements populated by the inferred operations being requested

    .. list-table::
       :header-rows: 1

       * - parameter
         - description
         - default
       * - **intersect**
         - true if span1 and span2 intersect, false otherwise
         - *optional*
       * - **combine**
         - the combined span of span1 and span 2
         - *optional*
       * - **split**
         - the split of span
         - *optional*
       * - **lat**
         - spherical latitude coordinate projected from the polar coordinate system, -90.0 to 90.0
         - *optional*
       * - **lon**
         - spherical longitude coordinate projected from the polar coordinate system, -180.0 to 180.0
         - *optional*
       * - **x**
         - polar x coordinate projected from the spherical coordinate system
         - *optional*
       * - **y**
         - polar y coordinate projected from the spherical coordinate system
         - *optional*

    **HTTP Example**

    .. code-block:: http

        HTTP/1.1 200 OK
        Server: sliderule/0.5.0
        Content-Type: text/plain
        Content-Length: 76


        {"y":1.1371580426033,"x":-0.20051164424058,"lat":29.999999999998,"lon":-100}



h5
--

``POST /source/h5 <request payload>``

    Reads a dataset from an HDF5 file and return the values of the dataset in a list.

    See `icesat2.h5 </web/rtd/api_reference/icesat2.html#h5>`_ function for a convient method for accessing HDF5 datasets.

**Request Payload** *(application/json)*

    .. list-table::
       :header-rows: 1

       * - parameter
         - description
         - default
       * - **asset**
         - data source asset (see `Assets <#assets>`_)
         - *required*
       * - **resource**
         - HDF5 filename
         - *required*
       * - **dataset**
         - full path to dataset variable
         - *required*
       * - **datatype**
         - the type of data the returned dataset values should be in
         - "DYNAMIC"
       * - **col**
         - the column to read from the dataset for a multi-dimensional dataset
         - 0
       * - **startrow**
         - the first row to start reading from in a multi-dimensional dataset
         - 0
       * - **numrows**
         - the number of rows to read when reading from a multi-dimensional dataset
         - -1 (all rows)
       * - **id**
         - value to echo back in the records being returned
         - 0

    **HTTP Example**

    .. code-block:: http

        POST /source/h5 HTTP/1.1
        Host: my-sliderule-server:9081
        Content-Length: 189


        {"asset": "atlas-local", "resource": "ATL03_20181019065445_03150111_003_01.h5", "dataset": "/gt1r/geolocation/segment_ph_cnt", "datatype": 2, "col": 0, "startrow": 0, "numrows": 5, "id": 0}


    **Python Example**

    .. code-block:: python

        >>> import sliderule
        >>> sliderule.set_url("slideruleearth.io")
        >>> asset = "icesat2"
        >>> resource = "ATL03_20181019065445_03150111_003_01.h5"
        >>> dataset = "/gt1r/geolocation/segment_ph_cnt"
        >>> rqst = {
        "asset" : asset,
        "resource": resource,
        "dataset": dataset,
        "datatype": sliderule.datatypes["INTEGER"],
        "col": 0,
        "startrow": 0,
        "numrows": 5,
        "id": 0
        }
        >>> rsps = sliderule.source("h5", rqst, stream=True)
        >>> print(rsps)
        [{'__rectype': 'h5dataset', 'datatype': 2, 'data': (245, 0, 0, 0, 7, 1, 0, 0, 17, 1, 0, 0, 1, 1, 0, 0, 4, 1, 0, 0), 'size': 20, 'offset': 0, 'id': 0}]

**Response Payload** *(application/octet-stream)*

    Serialized stream of H5 dataset records of the type ``h5dataset``.  See `De-serialization <./SlideRule.html#de-serialization>`_ for a description of how to process binary response records.




h5p
---

``POST /source/h5p <request payload>``

    Reads a list of datasets from an HDF5 file and returns the values of the datasets in a dictionary of lists.

    See `icesat2.h5p </web/rtd/api_reference/icesat2.html#h5p>`_ function for a convient method for accessing HDF5 datasets.

**Request Payload** *(application/json)*

    .. list-table::
       :header-rows: 1

       * - parameter
         - description
         - default
       * - **asset**
         - data source asset (see `Assets <#assets>`_)
         - *required*
       * - **resource**
         - HDF5 filename
         - *required*
       * - **datasets**
         - list of datasets (see `h5 <#h5>`_ for a list of parameters for each dataset)
         - *required*

    **Python Example**

    .. code-block:: python

        >>> import sliderule
        >>> sliderule.set_url("slideruleearth.io")
        >>> asset = "icesat2"
        >>> resource = "ATL03_20181019065445_03150111_003_01.h5"
        >>> dataset = "/gt1r/geolocation/segment_ph_cnt"
        >>> datasets = [ {"dataset": dataset, "col": 0, "startrow": 0, "numrows": 5} ]
        >>> rqst = {
        "asset" : asset,
        "resource": resource,
        "datasets": datasets,
        }
        >>> rsps = sliderule.source("h5p", rqst, stream=True)
        >>> print(rsps)
        [{'__rectype': 'h5file', 'dataset': '/gt1r/geolocation/segment_ph_cnt', 'elements': 5, 'size': 20, 'datatype': 2, 'data': (245, 0, 0, 0, 7, 1, 0, 0, 17, 1, 0, 0, 1, 1, 0, 0, 4, 1, 0, 0)}]

**Response Payload** *(application/octet-stream)*

    Serialized stream of H5 file data records of the type ``h5file``.  See `De-serialization <./SlideRule.html#de-serialization>`_ for a description of how to process binary response records.



health
------

``GET /source/health``

    Provides status on the health of the node.

**Response Payload** *(application/json)*

    JSON object containing a true|false indicator of the health of the node.

    .. code-block:: python

        {
            "healthy": true|false
        }



index
-----

``GET /source/index <request payload>``

    Return list of resources (i.e H5 files) that match the query criteria.

    Since the way resources are indexed (e.g. which meta-data to use), is very dependent upon the actual resources available; this endpoint is not necessarily
    useful in and of itself.  It is expected that data specific indexes will be built per SlideRule deployment, and higher level routines will be constructed
    that take advantage of this endpoint and provide a more meaningful interface.

**Request Payload** *(application/json)*

    .. code-block:: python

            {
                "or"|"and":
                {
                    "<index name>": { <index parameters>... }
                    ...
                }
            }

    .. list-table::
       :header-rows: 1

       * - parameter
         - description
         - default
       * - **index name**
         - name of server-side index to use (deployment specific)
         - *required*
       * - **index parameters**
         - an index span represented in the format native to the index selected
         - *required*


**Response Payload** *(application/json)*

    JSON object containing a list of the resources available to the SlideRule deployment that match the query criteria.

    .. code-block:: python

        {
            "resources": ["<resource name>", ...]
        }



metric
------

``GET /source/metric <request payload>``

    Return a list of metric values associated with a provided system attribute.

    Each SlideRule server node maintains internal metrics on a variety of things.  Each metric is associated with an attribute that identifies a set of metrics.

    When querying metrics, you provide the metric attribute, and the server will respond with the set of metrics associated with that attribute.

**Request Payload** *(application/json)*

    .. code-block:: python

      {
        "attr": <metric attribute>
      }

    .. list-table::
       :header-rows: 1

       * - parameter
         - description
         - default
       * - **metric attribute**
         - name of the attribute that is being queried
         - *required*


**Response Payload** *(application/json)*

    JSON object containing a set of the metric names and values.

    .. code-block:: python

        {
            "<metric name>": <metric value>,
            ...
        }



tail
----

``GET /source/tail <request payload>``

    Retrieve the most recent log messages generated by the server.

    The number of log message saved by the server is configured at startup.  This endpoint will return up to the maximum number of log messages that are saved.

**Request Payload** *(application/json)*

    .. code-block:: python

      {
        "monitor": "<monitor name>"
      }

    .. list-table::
       :header-rows: 1

       * - parameter
         - description
         - default
       * - **monitor**
         - name of the monitor to tail, should almost always be "EventMonitor"
         - *required*


**Response Payload** *(application/json)*

    JSON object containing a list of log messages.

    .. code-block:: python

        [
            "<log message 1>",
            "<log message 2>",
            ...
            "<log message N>"
        ]



time
-----

``GET /source/time <request payload>``

    Converts times from one format to another

**Request Payload** *(application/json)*

    .. list-table::
       :header-rows: 1

       * - parameter
         - description
         - default
       * - **time**
         - time value
         - *required*
       * - **input**
         - format of above time value: "NOW", "CDS", "GMT", "GPS"
         - *required*
       * - **output**
         - desired format of return value: same as above
         - *required*

    Sliderule supports the following time specifications

    NOW
        If supplied for either input or time then grab the current time

    CDS
        CCSDS 6-byte packet timestamp represented as [<day>, <ms>]

        days = 2 bytes of days since GPS epoch

        ms = 4 bytes of milliseconds in the current day

    GMT
        UTC time represented as "<year>:<day of year>:<hour in day>:<minute in hour>:<second in minute>"

    DATE
        UTC time represented as "<year>-<month>-<day of month>T<hour in day>:<minute in hour>:<second in minute>Z""

    GPS
        seconds since GPS epoch "January 6, 1980"


    **HTTP Example**

    .. code-block:: http

        GET /source/time HTTP/1.1
        Host: my-sliderule-server:9081
        Content-Length: 48


        {"time": "NOW", "input": "NOW", "output": "GPS"}

    **Python Example**

    .. code-block:: python

        rqst = {
            "time": "NOW",
            "input": "NOW",
            "output": "GPS"
        }

        rsps = sliderule.source("time", rqst)

**Response Payload** *(application/json)*

    JSON object describing the results of the time conversion

    .. code-block:: python

        {
            "time":     <time value>
            "format":   "<format of time value>"
        }


version
-------

``GET /source/version``

    Get the version information of the server.

**Response Payload** *(application/json)*

    JSON object containing the version information.

    .. code-block:: python

      {
          "server": {
              "packages": [
                  "<package 1>",
                  "<package 2>",
                  ...
                  "<package n>"
              ],
              "version": "<version string>",
              "launch": "<date of launch>",
              "commit": "<commit id of code>",
              "duration": <seconds since launch>
          }
          "<package 1>": {
              "version": "<version string>",
              "commit": "<commit id of code>"
          },
          "<package 2>": {
              "version": "<version string>",
              "commit": "<commit id of code>"
          },
          ...
          "<package n>": {
              "version": "<version string>",
              "commit": "<commit id of code>"
          }
      }
