===================
ICESat-2 Python API
===================

The ICESat-2 Python API is used to access the services provided by the ``sliderule-icesat2`` plugin for SlideRule. From Python, the module can be imported via:

.. code-block:: python

    from sliderule import icesat2

Polygons
########

All polygons provided to the ICESat-2 module functions must be provided as a list of dictionaries containing longitudes and latitudes in counter-clockwise order with the first and last point matching.

For example:

.. code-block:: python

    region = [ {"lon": -108.3435200747503, "lat": 38.89102961045247},
               {"lon": -107.7677425431139, "lat": 38.90611184543033}, 
               {"lon": -107.7818591266989, "lat": 39.26613714985466},
               {"lon": -108.3605610678553, "lat": 39.25086131372244},
               {"lon": -108.3435200747503, "lat": 38.89102961045247} ]

In order to facilitate other formats, the ``icesat2.toregion`` function can be used to convert polygons from the GeoJSON format to the format accepted by `SlideRule`.

Parameters
##########

When making a request to SlideRule to perform the ATL06-SR algorithm on segmented ATL03 data, there is a set of configurable parameters used by the algorithm to customize the processing performed and the results returned.
Not all parameters need to be defined when making a request; there are reasonable defaults used for each parameter so that only those parameters that want to be specifically customized need to be specified.

* ``"poly"``: polygon defining region of interest (see `polygons <#polygons>`_)
* ``"srt"``: surface type: 0-land, 1-ocean, 2-sea ice, 3-land ice, 4-inland water
* ``"cnf"``: confidence level for PE selection
* ``"ats"``: minimum along track spread
* ``"cnt"``: minimum PE count in segment
* ``"len"``: length of ATL06 segment in meters
* ``"res"``: step distance for successive ATL06 segments in meters
* ``"maxi"``: maximum iterations, not including initial least-squares-fit selection
* ``"H_min_win"``: minimum height of PE window in meters
* ``"sigma_r_max"``: maximum robust dispersion in meters

.. code-block:: python

    parms = {
        "cnf": 4,
        "ats": 20.0, 
        "cnt": 10,
        "len": 40.0,
        "res": 20.0,
        "maxi": 1
    }

Assets
######

When accessing SlideRule as a service, there are many times when you need to specify which source datasets it should use when processing the data.  
A source dataset is called an **asset** and is specified by its name as a string.

The asset name tells SlideRule where to get the data, and what format the data should be in. The following assets are supported by the ICESat-2 deployment of SlideRule:

.. csv-table:: 
    :header: asset, format, url, index

    atlas-s3,    s3,   icesat2-sliderule/data/ATLAS, empty.index
    atlas-local, file, /data/ATLAS,                  empty.index


Elevations
##########

The primary result returned by SlideRule for ICESat-2 processing requests is a set of gridden elevations corresponding to a geolocated ATL03 along-track segment.

The elevations are contained in a Python dictionary where each element in the dicitionary is a list of values.  
The values across all elements that occur at the same offset within their list go together.  This structure mimics a DataFrame and more naturally fits column-based analysis.

The result dictionary has the following elements:

- ``"segment_id"``: segment ID of first ATL03 segment in result
- ``"rgt"``: reference ground track
- ``"cycle"``: cycle
- ``"spot"``: laser spot 1 to 6
- ``"delta_time"``: seconds from GPS epoch (Jan 6, 1980)
- ``"lat"``: latitude (-90.0 to 90.0)
- ``"lon"``: longitude (-180.0 to 180.0)
- ``"h_mean"``: elevation in meters from ellipsoid
- ``"dh_fit_dx"``: along-track slope
- ``"dh_fit_dy"``: across-track slope

.. code-block:: python

    >>> rsps = icesat2.atl06(parms, resource, asset, as_numpy=False)
    >>> print(rsps["cycle"])
    [1, 1, 1, ... 1]


Functions
#########

init
----

""""""""""""""""

.. py:function:: icesat2.init (url, verbose=False, max_errors=3)

    Convenience function for initializing the underlying SlideRule module.  Must be called before other ICESat-2 API calls.
    This function is the same as calling the underlying sliderule functions: ``set_url``, ``set_verbose``, ``set_max_errors``.

    :param str url: the IP address or hostname of the SlideRule service (note, there is a special case where the url is provided as a list of strings instead of just a string; when a list is provided, the client hardcodes the set of servers that are used to process requests to the exact set provided; this is used for testing and for local installations and can be ignored by most users)
    :param bool verbose: whether or not user level log messages received from SlideRule generate a Python log message (see `sliderule.set_verbose <./SlideRule.html#set_verbose>`_)
    :param int max_errors: the number of errors returned by a SlideRule server before the client drops it from the available server list

    Example: 

    .. code-block:: python

        >>> from sliderule import icesat2
        >>> icesat2.init("my-sliderule-service.my-company.com", True)


cmr
---

""""""""""""""""

.. py:function:: icesat2.cmr(polygon, time_start=None, time_end=None, version='003', short_name='ATL03')

    Query the `NASA Common Metadata Repository (CMR) <https://cmr.earthdata.nasa.gov/search>`_ for a list of data within temporal and spatial parameters

    :param list polygon: polygon defining region of interest (see `polygons <#polygons>`_)
    :param str time_start: starting time for query in format ``<year>-<month>-<day>T<hour>:<minute>:<second>Z``
    :param str time_end: ending time for query in format ``<year>-<month>-<day>T<hour>:<minute>:<second>Z``
    :param str version: dataset version as found in the `NASA CMR Directory <https://cmr.earthdata.nasa.gov/search/site/collections/directory/eosdis>`_
    :param str short_name: dataset short name as defined in the `NASA CMR Directory <https://cmr.earthdata.nasa.gov/search/site/collections/directory/eosdis>`_
    :return: list of files (granules) for the dataset fitting the spatial and temporal parameters

    Example: 

    .. code-block:: python

        >>> from sliderule import icesat2
        >>> region = [ {"lon": -108.3435200747503, "lat": 38.89102961045247},
        ...            {"lon": -107.7677425431139, "lat": 38.90611184543033}, 
        ...            {"lon": -107.7818591266989, "lat": 39.26613714985466},
        ...            {"lon": -108.3605610678553, "lat": 39.25086131372244},
        ...            {"lon": -108.3435200747503, "lat": 38.89102961045247} ]
        >>> granules = icesat2.cmr(region)
        >>> granules
        ['ATL03_20181017222812_02950102_003_01.h5', 'ATL03_20181110092841_06530106_003_01.h5', ... 'ATL03_20201111102237_07370902_003_01.h5']



atl06
-----

""""""""""""""""

.. py:function:: icesat2.atl06(parms, resource, asset="atlas-s3", track=0, as_numpy=False)

    Performs ATL06-SR processing on ATL03 data and returns gridded elevations

    :param dict parms: parameters used to configure ATL06-SR algorithm processing (see `Parameters <#parameters>`_)
    :param str resource: ATL03 HDF5 filename
    :keyword str asset: data source asset (see `Assets <#assets>`_)
    :keyword int track: reference pair track number (1, 2, 3, or 0 to include for all three)
    :keyword bool as_numpy: when true returns results as flattened numpy arrays
    :return: list of gridded elevations (see `Elevations <#elevations>`_)

    Example: 

    .. code-block:: python

        >>> from sliderule import icesat2
        >>> icesat2.init("icesat2sliderule.org", True)
        >>> parms = { "cnf": 4, "ats": 20.0, "cnt": 10, "len": 40.0, "res": 20.0, "maxi": 1 }
        >>> resource = "ATL03_20181019065445_03150111_003_01.h5"
        >>> atl03_asset = "atlas-local"
        >>> rsps = icesat2.atl06(parms, resource, atl03_asset, as_numpy=False)
        >>> rsps["lat"][:5]
        [-78.99668681854881, -78.99649784368357, -78.99685968470074, -78.99649784368357, -78.99703874667964]
        >>> rsps["lon"][:5]
        [-63.97622360194674, -63.981167605752425, -63.976405240741286, -63.981167605752425, -63.97659412081279]
        >>> rsps["h_mean"][:5]
        [45.63628102552386, 45.61235614285759, 45.6602334215594, 45.60864517021007, 45.607383124478126]


atl06p
------

""""""""""""""""

.. py:function:: icesat2.atl06p(parm, asset="atlas-s3", track=0, as_numpy=False, max_workers=0, block=True)

    Performs ATL06-SR processing in parallel on ATL03 data and returns gridded elevations.  Unlike the `atl06 <#atl06>`_ function, 
    this function does not take a resource as a parameter; instead it is expected that the **parm** argument includes a polygon which
    is used to fetch all available resources from the CMR system automatically.

    Note, it is often the case that the list of resources (i.e. granules) returned by the CMR system includes granules that come close, but
    do not actually intersect the region of interest.  This is due to geolocation margin added to all CMR ICESat-2 resources in order to account
    for the spacecraft off-pointing.  The consequence is that SlideRule will return no data for some of the resources and issue a warning statement to that effect;
    this can be ignored and indicates no issue with the data processing.

    :param dict parms: parameters used to configure ATL06-SR algorithm processing (see `Parameters <#parameters>`_)
    :keyword str asset: data source asset (see `Assets <#assets>`_)
    :keyword int track: reference pair track number (1, 2, 3, or 0 to include for all three)
    :keyword bool as_numpy: when true returns results as flattened numpy arrays
    :keyword int max_workers: the number of threads to use when making concurrent requests to SlideRule (when set to 0, the number of threads is automatically and optimally determined based on the number of SlideRule server nodes available)
    :keyword bool block: wait for results to finish before returning; if set to false, instead of returning elevations, this function returns a list of concurrent futures)
    :return: list of gridded elevations (see `Elevations <#elevations>`_)


h5
--

""""""""""""""""

.. py:function:: icesat2.h5 (dataset, resource, asset="atlas-s3", datatype=sliderule.datatypes["REAL"], col=0, startrow=0, numrows=ALL_ROWS)

    Reads a dataset from an HDF5 file and returns the values of the dataset in a list

    This function provides an easy way for locally run scripts to get direct access to HDF5 data stored in a cloud environment.
    But it should be noted that this method is not the most efficient way to access remote H5 data, as the data is accessed one dataset at a time.
    Future versions may provide the ability to read multiple datasets at once, but in the meantime, if the user finds themselves needing direct 
    access to a lot of HDF5 data residing in the cloud, then use of the H5Coro Python package is recommended as it provides a native Python package
    for performant direct access to HDF5 data in the cloud.

    One of the difficulties in reading HDF5 data directly from a Python script is converting format of the data as it is stored in the HDF5 to a data
    format that is easy to use in Python.  The compromise that this function takes is that it allows the user to supply the desired data type of the 
    returned data via the **datatype** parameter, and the function will then return a **numpy** array of values with that data type.  
    
    There two possible ways to supply the data types: as a ``sliderule.datatypes`` enumeration, or as a hardcoded string. When the data type is supplied using the enumeration, the data type conversion occurs on the server side (unless the "DYNAMIC" value is supplied).  The possible enumeration values are:

    - ``sliderule.datatypes["TEXT"]``: return the data as a string of unconverted bytes
    - ``sliderule.datatypes["INTEGER"]``: return the data as an array of integers
    - ``sliderule.datatypes["REAL"]``: return the data as an array of double precision floating point numbers
    - ``sliderule.datatypes["DYNAMIC"]``: return the data in the numpy data type that is the closest match to the data as it is stored in the HDF5 file

    When the data type is supplied using a hardcoded string, the server reads the data as "DYNAMIC", meaning just the binary data is returned, and then inside the client, prior to return the results to the calling application, the data is converted using numpy on the binary string.  The possible hardcoded values are:

    - "INT8", "INT16", "INT32" ,"INT64" ,"UINT8" ,"UINT16" ,"UINT32" ,"UINT64" ,"BITFIELD" ,"FLOAT" ,"DOUBLE" ,"TIME8" ,"STRING"

    :param str dataset: full path to dataset variable (e.g. ``/gt1r/geolocation/segment_ph_cnt``)
    :param str resource: HDF5 filename
    :keyword str asset: data source asset (see `Assets <#assets>`_)
    :keyword int datatype: the type of data the returned dataset list should be in (datasets that are naturally of a different type undergo a best effort conversion to the specified data type before being returned)
    :keyword int col: the column to read from the dataset for a multi-dimensional dataset; if there are more than two dimensions, all remaining dimensions are flattened out when returned.
    :keyword int startrow: the first row to start reading from in a multi-dimensional dataset (or starting element if there is only one dimension)
    :keyword int numrows: the number of rows to read when reading from a multi-dimensional dataset (or number of elements if there is only one dimension); if **ALL_ROWS** selected, it will read from the **startrow** to the end of the dataset.
    :return: numpy array of dataset values

    Example: 

    .. code-block:: python

        segments    = icesat2.h5("/gt1r/land_ice_segments/segment_id",  resource, asset)
        heights     = icesat2.h5("/gt1r/land_ice_segments/h_li",        resource, asset)
        latitudes   = icesat2.h5("/gt1r/land_ice_segments/latitude",    resource, asset)
        longitudes  = icesat2.h5("/gt1r/land_ice_segments/longitude",   resource, asset)

        df = pd.DataFrame(data=list(zip(heights, latitudes, longitudes)), index=segments, columns=["h_mean", "latitude", "longitude"])


h5p
---

""""""""""""""""

.. py:function:: icesat2.h5p (datasets, resource, asset="atlas-s3")

    Reads a list of datasets from an HDF5 file and returns the values of the dataset in a dictionary of lists. 

    This function is considerably faster than the ``icesat2.h5`` function in that it not only reads the datasets in 
    parallel on the server side, but also shares a file context between the reads so that portions of the file that 
    need to be read multiple times do not result in multiple requests to S3.

    For a full discussion of the data type conversion options, see `h5 <ICESat-2.html#h5>`_.

    :param dict datasets: list of full paths to dataset variable (e.g. ``/gt1r/geolocation/segment_ph_cnt``); see below for additional parameters that can be added to each dataset
    :param str resource: HDF5 filename
    :keyword str asset: data source asset (see `Assets <#assets>`_)
    :return: dictionary of numpy arrays of dataset values, where the keys are the dataset names

    The ``datasets`` dictionary can optionally contain the following elements per entry:

    :keyword int valtype: the type of data the returned dataset list should be in (datasets that are naturally of a different type undergo a best effort conversion to the specified data type before being returned)
    :keyword int col: the column to read from the dataset for a multi-dimensional dataset; if there are more than two dimensions, all remaining dimensions are flattened out when returned.
    :keyword int startrow: the first row to start reading from in a multi-dimensional dataset (or starting element if there is only one dimension)
    :keyword int numrows: the number of rows to read when reading from a multi-dimensional dataset (or number of elements if there is only one dimension); if **ALL_ROWS** selected, it will read from the **startrow** to the end of the dataset.

    Example: 

    .. code-block:: python

        >>> from sliderule import icesat2
        >>> icesat2.init(["127.0.0.1"], False)
        >>> datasets = [
        ...         {"dataset": "/gt1l/land_ice_segments/h_li", "numrows": 5},
        ...         {"dataset": "/gt1r/land_ice_segments/h_li", "numrows": 5},
        ...         {"dataset": "/gt2l/land_ice_segments/h_li", "numrows": 5},
        ...         {"dataset": "/gt2r/land_ice_segments/h_li", "numrows": 5},
        ...         {"dataset": "/gt3l/land_ice_segments/h_li", "numrows": 5},
        ...         {"dataset": "/gt3r/land_ice_segments/h_li", "numrows": 5}
        ...     ]
        >>> rsps = icesat2.h5p(datasets, "ATL06_20181019065445_03150111_003_01.h5", "atlas-local")
        >>> print(rsps)
        {'/gt2r/land_ice_segments/h_li': array([45.3146427 , 45.27640582, 45.23608027, 45.21131015, 45.15692304]), 
         '/gt2l/land_ice_segments/h_li': array([45.35118977, 45.33535027, 45.27195617, 45.21816889, 45.18534204]), 
         '/gt1l/land_ice_segments/h_li': array([45.68811156, 45.71368944, 45.74234326, 45.74614113, 45.79866465]), 
         '/gt3l/land_ice_segments/h_li': array([45.29602321, 45.34764226, 45.31430979, 45.31471701, 45.30034622]), 
         '/gt1r/land_ice_segments/h_li': array([45.72632446, 45.76512574, 45.76337375, 45.77102473, 45.81307948]), 
         '/gt3r/land_ice_segments/h_li': array([45.14954134, 45.18970635, 45.16637644, 45.15235916, 45.17135806])}


toregion
--------

""""""""""""""""

.. py:function:: icesat2.toregion (geojson, as_file=True)

    Convert a GeoJSON representation of a geospatial region into a lat,lon list recognized by SlideRule

    :param str geojson: GeoJSON formatted region of interest
    :param bool as_file: when true the **geojson** parameter is treated as a filename; when false the geojson parameter is treated as a string containing the GeoJSON specification
    :return: list structure containing the region of interest that can be used for the **poly** parameter in a processing request to SlideRule

    Example: 

    .. code-block:: python

        from sliderule import icesat2

        # Region of Interest #
        region_filename = sys.argv[1]
        region = icesat2.toregion(region_filename)

        # Configure SlideRule #
        icesat2.init("icesat2sliderule.org", False)

        # Build ATL06 Request #
        parms = {
            "poly": region,
            "srt": icesat2.SRT_LAND,
            "cnf": icesat2.CNF_SURFACE_HIGH,
            "ats": 10.0,
            "cnt": 10,
            "len": 40.0,
            "res": 20.0,
            "maxi": 1
        }

        # Get ATL06 Elevations
        atl06 = process_atl06_algorithm(parms, "atlas-s3")



Endpoints
#########

atl06
-----

""""""""""""""""

``POST /source/atl06 <request payload>``

    Perform ATL06-SR processing on ATL03 data and return gridded elevations

**Request Payload** *(application/json)*

    .. list-table::
       :header-rows: 1
    
       * - parameter
         - description
         - default
       * - **atl03-asset**
         - data source (see `Assets <#assets>`_)
         - atlas-local
       * - **resource**
         - ATL03 HDF5 filename
         - *required*     
       * - **track**
         - track number: 1, 2, 3, or 0 for all three tracks
         - 0
       * - **parms**
         - ATL06-SR algorithm processing configuration (see `Parameters <#parameters>`_)
         - *required*
       * - **timeout**
         - number of seconds to wait for first response
         - wait forever

    **HTTP Example**

    .. code-block:: http

        POST /source/atl06 HTTP/1.1
        Host: my-sliderule-server:9081
        Content-Length: 179

        {"atl03-asset": "atlas-local", "resource": "ATL03_20181019065445_03150111_003_01.h5", "track": 0, "parms": {"cnf": 4, "ats": 20.0, "cnt": 10, "len": 40.0, "res": 20.0, "maxi": 1}}

    **Python Example**

    .. code-block:: python

        # Build ATL06 Parameters
        parms = { 
            "cnf": 4,
            "ats": 20.0,
            "cnt": 10,
            "len": 40.0,
            "res": 20.0,
            "maxi": 1 
        }

        # Build ATL06 Request
        rqst = {
            "atl03-asset" : "atlas-local",
            "resource": "ATL03_20181019065445_03150111_003_01.h5",
            "track": 0,
            "parms": parms
        }

        # Execute ATL06 Algorithm
        rsps = sliderule.source("atl06", rqst, stream=True)

**Response Payload** *(application/octet-stream)*

    Serialized stream of gridded elevations of type ``atl06rec``.  See `De-serialization <./SlideRule.html#de-serialization>`_ for a description of how to process binary response records.



indexer
-------

""""""""""""""""

``POST /source/indexer <request payload>``

    Return a set of meta-data index records for each ATL03 resource (i.e. H5 file) listed in the request.  
    Index records are used to create local indexes of the resources available to be processed,
    which in turn support spatial and temporal queries.
    Note, while SlideRule supports native meta-data indexing, this feature is typically not used in favor of accessing the
    NASA CMR system directly.

**Request Payload** *(application/json)*

    .. list-table::
       :header-rows: 1
    
       * - parameter
         - description
         - default
       * - **atl03-asset**
         - data source (see `Assets <#assets>`_)
         - atlas-local
       * - **resources**
         - List of ATL03 HDF5 filenames
         - *required*     
       * - **timeout**
         - number of seconds to wait for first response
         - wait forever
    
    **HTTP Example**

    .. code-block:: http

        POST /source/indexer HTTP/1.1
        Host: my-sliderule-server:9081
        Content-Length: 131

        {"atl03-asset": "atlas-local", "resources": ["ATL03_20181019065445_03150111_003_01.h5", "ATL03_20190512123214_06760302_003_01.h5"]}

    **Python Example**

    .. code-block:: python

        # Build Indexer Request
        rqst = {
            "atl03-asset" : "atlas-local",
            "resources": ["ATL03_20181019065445_03150111_003_01.h5", "ATL03_20190512123214_06760302_003_01.h5"],
        }

        # Execute ATL06 Algorithm
        rsps = sliderule.source("indexer", rqst, stream=True)

**Response Payload** *(application/octet-stream)*

    Serialized stream of ATL03 meta-data index records of type ``atl03rec.index``.  See `De-serialization <./SlideRule.html#de-serialization>`_ for a description of how to process binary response records.
