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
###################

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
    atl03-s3,    s3,   icesat2-sliderule/data/ATL03, empty.index
    atl06-s3,    s3,   icesat2-sliderule/data/ATL06, empty.index
    atlas-local, file, /data/ATLAS,                  empty.index
    atl03-local, file, /data/ATL03,                  empty.index
    atl06-local, file, /data/ATL06,                  empty.index


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
        >>> icesat2.init(["127.0.0.1"], True)
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

Reads a dataset from an HDF5 file and return the values of the dataset

.. code-block:: python

    values = icesat2.h5(dataset, resource, asset="atlas-s3",
        datatype=sliderule.datatypes["REAL"])

Arguments
---------

- `dataset`: full path to dataset variable (e.g. ``/gt1r/geolocation/segment_ph_cnt``)
- `resource`: ATL03 HDF5 filename

Keyword Arguments
-----------------

- `asset` : ``"atlas-local"``, ``"atlas-s3"``
- `datatype`: input variable datatype

    * ``sliderule.datatypes["TEXT"]``
    * ``sliderule.datatypes["INTEGER"]``
    * ``sliderule.datatypes["REAL"]``
    * ``sliderule.datatypes["DYNAMIC"]``

Returns
-------

- ``"id"``: echoed data id
- ``"dataset"``: echoed data name
- ``"datatype"``: data type (``"TEXT"``, ``"INTEGER"``, ``"REAL"``)
- ``"offset"``: fragment byte offset of returned data stream
- ``"size"``: size in bytes of fragment
- ``"data[]"``: array of bytes representing dataset values of the specified type
