======================
ICESat-2 Plugin Module
======================


1. Overview
===========

The ICESat-2 API queries a set of ATL03 input granules for photon heights and locations based on a set of photon-input parameters that select the geographic and temporal extent of the request.  It then selects a subset of these photons based on a set of photon classification parameters, and divides these selected photons into short along-track extents, each of which is suitable for generating a single height estimate.  These extents may be returned to the python client, or may be passed to the ATL06 height-estimation module.  The ATL06 height-estimation module fits line segments to the photon heights and locations in each extent, and selects segments based on their quality to return to the user.


2. Parameters
=============

The ICESat-2 module provides additional parameters specific to making ICESat-2 processing requests.


2.1 Photon-input parameters
---------------------------

The photon-input parameters allow the user to select an area, a time range, or a specific ATL03 granule to use for input to the photon-selection algorithm.  If multiple parameters are specified, the result will be those photons that match all of the parameters.

* ``"poly"``: polygon defining region of interest (see `polygons <#id1>`_)
* ``"raster"``: geojson describing region of interest which enables rasterized subsetting on servers (see `geojson <#id3>`_)
* ``"track"``: reference pair track number (1, 2, 3, or 0 to include for all three; defaults to 0)
* ``"rgt"``: reference ground track (defaults to all if not specified)
* ``"cycle"``: counter of 91-day repeat cycles completed by the mission (defaults to all if not specified)
* ``"region"``: geographic region for corresponding standard product (defaults to all if not specified)
* ``"t0"``: start time for filtering granules (format %Y-%m-%dT%H:%M:%SZ, e.g. 2018-10-13T00:00:00Z)
* ``"t1"``: stop time for filtering granuels (format %Y-%m-%dT%H:%M:%SZ, e.g. 2018-10-13T00:00:00Z)


2.1.1 Time Conversions
#######################

Result times returned by SlideRule for ICESat-2 are in standard GPS units (fractional seconds since the GPS epoch).  To convert standard GPS time to an ATLAS SDP time, you need to subtract the number of seconds between the GPS epoch which is January 6, 2018 at midnight (1980-01-06T00:00:00.000000Z) and the ATLAS SDP epoch of January 1, 2018 at midnight (2018-01-01T00:00:00.000000Z). That number is ``1198800018`` seconds.


2.2 Photon-selection parameters
--------------------------------

Once the ATL03 input data are are selected, a set of photon-selection photon parameters are used to select from among the available photons.  At this stage, additional photon-classification algorithms (ATL08, YAPC) may be selected beyond what is available in the ATL03 files.  The criterial described by these parameters are applied together, so that only photons that fulfill all of the requirements are returned.

2.2.1 Native ATL03 photon classification
##########################################

ATL03 contains a set of photon classification values, that are designed to identify signal photons for different surface types with specified confidence:

* ``"srt"``: surface type: 0-land, 1-ocean, 2-sea ice, 3-land ice, 4-inland water
* ``"cnf"``: confidence level for photon selection, can be supplied as a single value (which means the confidence must be at least that), or a list (which means the confidence must be in the list)
* ``"quality_ph"``: quality classification based on an ATL03 algorithms that attempt to identify instrumental artifacts, can be supplied as a single value (which means the classification must be exactly that), or a list (which means the classification must be in the list).

The signal confidence can be supplied as strings {"atl03_tep", "atl03_not_considered", "atl03_background", "atl03_within_10m", "atl03_low", "atl03_medium", "atl03_high"} or as numbers {-2, -1, 0, 1, 2, 3, 4}.
The default values, of srt=3, cnf=0, and quality_ph=0 will return all photons not flagged as instrumental artifacts, and the 'cnf' parameter will match the land-ice classification.

2.2.2 ATL08 classification
##########################################

If ATL08 classification parameters are specified, the ATL08 (vegetation height) files corresponding to the ATL03 files are queried for the more advanced classification scheme available in those files.  Photons are then selected based on the classification values specified.  Note that srt=0 (land) and cnf=0 (no native filtering) should be specified to allow all ATL08 photons to be used.

* ``"atl08_class"``: list of ATL08 classifications used to select which photons are used in the processing (the available classifications are: "atl08_noise", "atl08_ground", "atl08_canopy", "atl08_top_of_canopy", "atl08_unclassified")

2.2.3 YAPC classification
##########################################

The experimental YAPC (Yet Another Photon Classifier) photon-classification scheme assigns each photon a score based on the number of adjacent photons.  YAPC parameters are provided as a dictionary, with entries described below:

* ``"yapc"``: settings for the yapc algorithm; if provided then SlideRule will execute the YAPC classification on all photons
    - ``"score"``: the minimum yapc classification score of a photon to be used in the processing request
    - ``"knn"``: the number of nearest neighbors to use, or specify 0 to allow automatic selection of the number of neighbors (recommended)
    - ``"win_h"``: the window height used to filter the nearest neighbors
    - ``"win_x"``: the window width used to filter the nearest neighbors
    - ``"version"``: the version of the YAPC algorithm to use

To run the YAPC algorithm, specify the YAPC settings as a sub-dictionary. Here is an example set of parameters that runs YAPC:

.. code-block:: python

    parms = {
        "cnf": 0,
        "yapc": { "score": 0, "knn": 4 },
        "ats": 10.0,
        "cnt": 5,
        "len": 20.0,
        "res": 20.0,
        "maxi": 1
    }

2.3 Photon-extent parameters
----------------------------

Selected photons are collected into extents, each of which may be suitable for elevation fitting.  The _len_ parameter specifies the length of each extent, and the _res_parameter specifies the distance between subsequent extent centers.  If _res_ is less than _len_, subsequent segments will contain duplicate photons.  The API may also select photons based on their along-track distance, or based on the segment-id parameters in the ATL03 product (see the _dist_in_seg_ parameter).

* ``"len"``: length of each extent in meters
* ``"res"``: step distance for successive extents in meters
* ``"dist_in_seg"``: true|false flag indicating that the units of the ``"len"`` and ``"res"`` are in ATL03 segments (e.g. if true then a len=2 is exactly 2 ATL03 segments which is approximately 40 meters)

Extents are optionally filtered based on the number of photons in each extent and the distribution of those photons.  If the ``"pass_invalid"`` parameter is set to _False_, only those extents fulfilling these criteria will be returned.

* ``"pass_invalid"``: true|false flag indicating whether or not extents that fail validation checks are still used and returned in the results
* ``"ats"``: minimum along track spread
* ``"cnt"``: minimum photon count in segment

2.4 ATL06-SR algorithm parameters
---------------------------------

The ATL06-SR algorithm fits a line segment to the photons in each extent, using an iterative selection refinement to eliminate noise photons not correctly identified by the photon classification.  The results are then checked against three parameters : ''"sigma_r_max"'', which eliminates segments for which the robust dispersion of the residuals is too large, and the ``"ats"`` and ``"cnt"`` parameters described above, which eliminate segments for which the iterative fitting has eliminated too many photons.  The optional ''"compact"'' instructs the algorithm to return a minimal subset of ATL06-SR segment parameters.

* ``"maxi"``: maximum iterations, not including initial least-squares-fit selection
* ``"H_min_win"``: minimum height to which the refined photon-selection window is allowed to shrink, in meters
* ``"sigma_r_max"``: maximum robust dispersion in meters
* ``"compact"``: return compact version of results (leaves out most metadata)

2.5 Ancillary field parameters
------------------------------

The ancillary field parameters allow the user to request additional fields from the ATL03 granule to be returned with the photon extent and ATL06-SR elevation responses.  Each field provided by the user will result in a corresponding column added to the returned GeoDataFrame.

* ``"atl03_geo_fields"``: fields in the "geolocation" and "geophys_corr" groups of the ATL03 granule
* ``"atl03_ph_fields"``: fields in the "heights" group of the ATL03 granule

For example:

.. code-block:: python

    parms = {
        "atl03_geo_fields":     ["solar_elevation"],
        "atl03_ph_fields":      ["pce_mframe_cnt"]
    }

2.6 PhoREAL parameters (feature preview)
----------------------------------------

.. warning::
    The PhoREAL capabilities in SlideRule are still in development and are subject to change with each release until it is officially supported.

The PhoREAL vegetation algorithm, developed at the University of Texas at Austin, provides vegatation statistics over custom-length ATL03 photon segments.  A subset of these algorithms have been integrated into SlideRule are accessed via the _atl08_ and _atl08p_ APIs using the ``"phoreal"`` parameter set.

To enable PhoREAL functionality, the ``"phoreal"`` parameter must be populated in the request dictionary.

* ``"phoreal"``: dictionary of rasters to sample
    - ``"binsize"``: size of the veritical photon bin in meters
    - ``"geoloc"``: algorithm to use to calculate the geolocation (latitude, longitude, along-track distance, and time) of each custom length PhoREAL segment; "mean" - takes the average value across all photons in the segment; "median" - takes the median value across all photons in the segment; "center" - takes the halfway value calculated by the average of the first and last photon in the segment
    - ``"use_abs_h"``: boolean whether the absoluate photon heights are used instead of the normalized heights
    - ``"send_waveform"``: boolean whether to send to the client the photon height histograms in addition to the vegetation statistics
    - ``"above_classifier"``: boolean whether to use the ABoVE photon classifier when determining top of canopy photons

Note: when PhoREAL is enabled, the ATL03 extent records (_atl03rec_) are enhanced to include the following populated fields:

* ``"relief"``: ATL08 normalized photon heights
* ``"landcover"``: ATL08 landcover flags
* ``"snowcover"``: ATL08 snowcover flags


2.7 Parameter reference table
------------------------------

The default set of parameters used by SlideRule are set to match the ICESat-2 project ATL06 settings as close as possible.
To obtain fewer false-positive returns, this set of parameters can be modified with cnf=3 or cnf=4.

.. list-table:: ICESat-2 Request Parameters
   :widths: 25 25 50
   :header-rows: 1

   * - Parameter
     - Units
     - Default
   * - ``"atl03_geo_fields"``
     - String/List
     -
   * - ``"atl03_ph_fields"``
     - String/List
     -
   * - ``"atl08_class"``
     - Integer/List or String/List
     -
   * - ``"ats"``
     - Float - meters
     - 20.0
   * - ``"cnf"``
     - Integer/List or String/List
     - 1 (within 10m)
   * - ``"cnt"``
     - Integer
     - 10
   * - ``"compact"``
     - Boolean
     - False
   * - ``"cycle"``
     - Integer - orbit cycle
     -
   * - ``"dist_in_seg"``
     - Boolean
     - False
   * - ``"H_min_win"``
     - Float - meters
     - 3.0
   * - ``"len"``
     - Float - meters
     - 40.0
   * - ``"maxi"``
     - Integer
     - 5
   * - ``"pass_invalid"``
     - Boolean
     - False
   * - ``"phoreal.above_classifier"``
     - Boolean
     - False
   * - ``"phoreal.binsize"``
     - Float - meters
     - 1.0
   * - ``"phoreal.geoloc"``
     - String
     - "median"
   * - ``"phoreal.send_waveform"``
     - Boolean
     - False
   * - ``"phoreal.use_abs_h"``
     - Boolean
     - False
   * - ``"quality_ph"``
     - Integer/List or String/List
     - 0 (nominal)
   * - ``"res"``
     - Float - meters
     - 20.0
   * - ``"rgt"``
     - Integer - reference ground track
     -
   * - ``"sigma_r_max"``
     - Float
     - 5.0
   * - ``"srt"``
     - Integer
     - 3 (land ice)
   * - ``"track"``
     - Integer - 0: all tracks, 1: gt1, 2: gt2, 3: gt3
     - 0
   * - ``"yapc.knn"``
     - Integer
     - 0 (calculated)
   * - ``"yapc.score"``
     - Integer - 0 to 255
     - 0
   * - ``"yapc.win_h"``
     - Float - meters
     - 6.0
   * - ``"yapc.win_x"``
     - Float - meters
     - 15.0
   * - ``"yapc.version"``
     - Integer: 1 and 2: v2 algorithm, 3: v3 algorithm
     - 3


3. Returned data
=========================

Three main kinds of data are returned by the ICESat-2 APIs: segmented photon data (from the ATL03 and ATL03p algorithms), elevation data (from the ATL06 and ATL06p algorithms), and vegetation data (from the ATL08 and ATL08p algorithms).

All data returned by the ICESat-2 APIs are organized around the concept of an ``extent``.  An extent is a variable length, customized ATL03 segment.  It takes the ATL03 photons and divides them up based on their along-track distance, filters them, and then packages them together a single new custom segment.  Given that the ICESat-2 standard data products have a well defined meaning for segment, SlideRule uses the term extent to indicate this custom-length and custom-filtered segment of photons.

Each extent is uniquely identified by an extent ID. The extent ID is analogous to the ATL03 segment ID, and is consistently generated for any extent given the same input parameters.  This means subsequent runs of SlideRule with the same request parameters will return the same extent IDs.

NOTE - while all data returned from SlideRule for ATL03/06/08 endpoints include the extent ID (as ``"extent_id"``), by default the Python client strips it out when it creates the final GeoDataFrame. There is an option to keep the extend ID by setting the "keep_id" argument in the atl03/06/08 group of Python functions to True.  This is useful when performing merges on GeoDataFrames from multiple APIs (for example, you can combine results from atl06 and atl08 endpoints and created a single GeoDataFrame with both elevation and vegatation data in it).


3.1 Segmented Photon Data
--------------------------

The photon data is stored as along-track segments inside the ATL03 granules, which is then broken apart by SlideRule and re-segmented according to processing
parameters supplied at the time of the request. The new segments are called **extents**.  When the length of an extent is 40 meters, and the step size is 20 meters, the extent matches the ATL06 segments.

Most of the time, the photon extents are kept internal to SlideRule and not returned to the user.  But there are some APIs that do return raw photon extents for the user to process on their own.
Even though this offloads processing on the server, the API calls can take longer since more data needs to be returned to the user, which can bottleneck over the network.

Photon extents are returned as GeoDataFrames where each row is a photon.  Each extent represents the data that the ATL06 algorithm uses to generate a single ATL06 elevation.
When the step size is shorter than the length of the extent, the extents returned overlap eachother which means that each photon is being returned multiple times and will be duplicated in the resulting GeoDataFrame.

The GeoDataFrame for each photon extent has the following columns:

- ``"track"``: reference pair track number (1, 2, 3)
- ``"sc_orient"``: spacecraft orientation (0: backwards, 1: forwards)
- ``"rgt"``: reference ground track
- ``"cycle"``: cycle
- ``"segment_id"``: segment ID of first ATL03 segment in result
- ``"segment_dist"``: along track distance from the equator to the center of the extent (in meters)
- ``"count"``: the number of photons in the segment
- ``"delta_time"``: seconds from ATLAS Standard Data Product (SDP) epoch (Jan 1, 2018)
- ``"latitude"``: latitude (-90.0 to 90.0)
- ``"longitude"``: longitude (-180.0 to 180.0)
- ``"distance"``: along track distance of the photon in meters (with respect to the center of the segment)
- ``"height"``: height of the photon in meters
- ``"solar_elevation"``: solar elevation from ATL03 at time of measurement, in degrees
- ``"background_rate"``: background photon counts per second
- ``"atl08_class"``: the photon's ATL08 classification (0: noise, 1: ground, 2: canopy, 3: top of canopy, 4: unclassified)
- ``"atl03_cnf"``: the photon's ATL03 confidence level (-2: TEP, -1: not considered, 0: background, 1: within 10m, 2: low, 3: medium, 4: high)
- ``"quality_ph"``: the photon's quality classification (0: nominal, 1: possible after pulse, 2: possible impulse responpse effect, 3: possible tep)
- ``"yapc_score"``: the photon's YAPC classification (0 - 255, the larger the number the higher the confidence in surface reflection)


3.2 Elevations
--------------

The primary result returned by SlideRule for ICESat-2 processing requests is a set of geolocated elevations corresponding to a geolocated ATL03 along-track segment. The elevations are contained in a GEoDataFrame where each row represents a calculated elevation.

Elements that are present in the **compact** version of the results are noted below.

The elevation GeoDataFrame has the following columns:

- ``"segment_id"``: segment ID of first ATL03 segment in result
- ``"n_fit_photons"``: number of photons used in final calculation
- ``"pflags"``: processing flags (0x1 - spread too short; 0x2 - too few photons; 0x4 - max iterations reached)
- ``"rgt"``: reference ground track
- ``"cycle"``: cycle
- ``"spot"``: laser spot 1 to 6
- ``"gt"``: ground track (10: GT1L, 20: GT1R, 30: GT2L, 40: GT2R, 50: GT3L, 60: GT3R)
- ``"distance"``: along track distance from the equator in meters
- ``"delta_time"``: seconds from ATLAS Standard Product epoch (Jan 1, 2018) [*in compact*]
- ``"lat"``: latitude (-90.0 to 90.0) [*in compact*]
- ``"lon"``: longitude (-180.0 to 180.0) [*in compact*]
- ``"h_mean"``: elevation in meters from ellipsoid [*in compact*]
- ``"dh_fit_dx"``: along-track slope
- ``"dh_fit_dy"``: across-track slope
- ``"w_surface_window_final"``: width of the window used to select the final set of photons used in the calculation
- ``"rms_misfit"``: measured error in the linear fit of the surface
- ``"h_sigma"``: error estimate for the least squares fit model


4. Callbacks
=============
For large processing requests, it is possible that the data returned from the API is too large or impractical to fit in the local memory of the Python interpreter making the request.
In these cases, certain APIs in the SlideRule Python client allow the calling application to provide a callback function that is called for every result that is returned by the servers.
If a callback is supplied, the API will not return back to the calling application anything associated with the supplied record types, but assumes the callback fully handles processing the data.
The callback function takes the following form:

.. py:function:: callback (record)

    Callback that handles the results of a processing request for the given record.

    :param dict record: the record object, usually a dictionary containing data

Here is an example of a callback being used for the ``atl03sp`` function:

    .. code-block:: python

        rec_cnt = 0
        ph_cnt = 1

        def atl03rec_cb(rec):
            global rec_cnt, ph_cnt
            rec_cnt += 1
            ph_cnt += rec["count"][0] + rec["count"][1]
            print("{} {}".format(rec_cnt, ph_cnt), end='\r')

        gdf = icesat2.atl03sp({}, callbacks = {"atl03rec": atl03rec_cb})



5. Endpoints
=============

atl06
-----

``POST /source/atl06 <request payload>``

    Perform ATL06-SR processing on ATL03 data and return geolocated elevations

**Request Payload** *(application/json)*

    .. list-table::
       :header-rows: 1

       * - parameter
         - description
         - default
       * - **resource**
         - ATL03 HDF5 filename
         - *required*
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

        {"resource": "ATL03_20181019065445_03150111_003_01.h5", "parms": {"asset": "atlas-local", "cnf": 4, "ats": 20.0, "cnt": 10, "len": 40.0, "res": 20.0, "maxi": 1}}

    **Python Example**

    .. code-block:: python

        # Build ATL06 Parameters
        parms = {
            "asset": "atlas-local",
            "cnf": 4,
            "ats": 20.0,
            "cnt": 10,
            "len": 40.0,
            "res": 20.0,
            "maxi": 1
        }

        # Build ATL06 Request
        rqst = {
            "resource": "ATL03_20181019065445_03150111_003_01.h5",
            "parms": parms
        }

        # Execute ATL06 Algorithm
        rsps = sliderule.source("atl06", rqst, stream=True)

**Response Payload** *(application/octet-stream)*

    Serialized stream of geolocated elevations of type ``atl06rec``.  See `De-serialization <./SlideRule.html#de-serialization>`_ for a description of how to process binary response records.



atl03s
------

``POST /source/atl03s <request payload>``

    Subset ATL03 data and return segments of photons

**Request Payload** *(application/json)*

    .. list-table::
       :header-rows: 1

       * - parameter
         - description
         - default
       * - **resource**
         - ATL03 HDF5 filename
         - *required*
       * - **parms**
         - ATL06-SR algorithm processing configuration (see `Parameters <#parameters>`_)
         - *required*
       * - **timeout**
         - number of seconds to wait for first response
         - wait forever

    **HTTP Example**

    .. code-block:: http

        POST /source/atl03s HTTP/1.1
        Host: my-sliderule-server:9081
        Content-Length: 134

        {"resource": "ATL03_20181019065445_03150111_003_01.h5", "parms": {"asset": "atlas-local", "len": 40.0, "res": 20.0}}

    **Python Example**

    .. code-block:: python

        # Build ATL06 Parameters
        parms = {
            "asset": "atlas-local",
            "len": 40.0,
            "res": 20.0,
        }

        # Build ATL06 Request
        rqst = {
            "resource": "ATL03_20181019065445_03150111_003_01.h5",
            "parms": parms
        }

        # Execute ATL06 Algorithm
        rsps = sliderule.source("atl03s", rqst, stream=True)

**Response Payload** *(application/octet-stream)*

    Serialized stream of photon segments of type ``atl03rec``.  See `De-serialization <./SlideRule.html#de-serialization>`_ for a description of how to process binary response records.



indexer
-------

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

        {"asset": "atlas-local", "resources": ["ATL03_20181019065445_03150111_003_01.h5", "ATL03_20190512123214_06760302_003_01.h5"]}

    **Python Example**

    .. code-block:: python

        # Build Indexer Request
        rqst = {
            "asset" : "atlas-local",
            "resources": ["ATL03_20181019065445_03150111_003_01.h5", "ATL03_20190512123214_06760302_003_01.h5"],
        }

        # Execute ATL06 Algorithm
        rsps = sliderule.source("indexer", rqst, stream=True)

**Response Payload** *(application/octet-stream)*

    Serialized stream of ATL03 meta-data index records of type ``atl03rec.index``.  See `De-serialization <./SlideRule.html#de-serialization>`_ for a description of how to process binary response records.
