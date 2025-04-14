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

* ``"poly"``: polygon defining region of interest (see `polygons </web/rtd/user_guide/SlideRule.html#polygons>`_)
* ``"region_mask"``: geojson describing region of interest which enables rasterized subsetting on servers (see `geojson </web/rtd/user_guide/SlideRule.html#geojson>`_)
* ``"track"``: reference pair track number (1, 2, 3, or 0 to include for all three; defaults to 0); note that this overrides the beam selection
* ``"beam"``: list of beam identifiers (gt1l, gt1r, gt2l, gt2r, gt3l, gt3r; defaults to all)
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
        "res": 20.0
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

The ATL06-SR algorithm fits a line segment to the photons in each extent, using an iterative selection refinement to eliminate noise photons not correctly identified by the photon classification.  The results are then checked against three parameters : ''"sigma_r_max"'', which eliminates segments for which the robust dispersion of the residuals is too large, and the ``"ats"`` and ``"cnt"`` parameters described above, which eliminate segments for which the iterative fitting has eliminated too many photons.

* ``"maxi"``: maximum iterations, not including initial least-squares-fit selection
* ``"H_min_win"``: minimum height to which the refined photon-selection window is allowed to shrink, in meters
* ``"sigma_r_max"``: maximum robust dispersion in meters

2.5 Ancillary field parameters
------------------------------

The ancillary field parameters allow the user to request additional fields from the source datasets being subsetted.
The ``"atl03_geo_fields"``, ``"atl03_ph_fields"``, and ``"atl08_fields"`` are used to specify additional fields in the ATL03 and ATL08 granules to be returned with the photon extent and ATL06-SR elevation responses.
The ``"atl06_fields"`` is used to specify additional fields in the ATL06 granule for ATL06 subsetting requests.
Each field provided by the user will result in a corresponding column added to the returned GeoDataFrame.  Note: if a field is requested that is already present in the default GeoDataFrame, then the name of both fields will be changed to include a _x suffix for the default incusion of the field, and a _y for the ancillary inclusion of the field.  In general, they should have the same value, but in some cases the ancillary field goes through different processing steps and may possibly contain a different value.

* ``"atl03_geo_fields"``: fields in the "geolocation" and "geophys_corr" groups of the ATL03 granule
* ``"atl03_ph_fields"``: fields in the "heights" group of the ATL03 granule
* ``"atl06_fields"``: fields in the "land_ice_segments" group of the ATL06 granule
* ``"atl08_fields"``: fields in the "land_segments" group of the ATL08 granule

For example:

.. code-block:: python

    parms = {
        "atl03_geo_fields":     ["solar_elevation"],
        "atl03_ph_fields":      ["pce_mframe_cnt"],
        "atl06_fields":         ["ground_track/ref_azimuth"],
        "atl08_fields":         ["asr"]
    }

2.5.1 ATL03 Subsetted Ancillary Data
####################################

Ancillary data returned from the ``"atl03s"`` and ``"atl03sp"`` APIs are per-photon values that are read from the ATL03 granules.  No processing is performed on the data read out of the ATL03 granule.  The fields must come from either a per-photon variable (atl03_ph_fields), or a per-segment variable (atl03_geo_fields).

2.5.2 ATL06-SR Ancillary Data
#############################

Ancillary data returned from the ``"atl06"`` and ``"atl06p"`` APIs come from the ancillary fields specified for ATL03, but instead of being returned as-is, they are processed using the ATL06 least-squares-fit algorithm, and only the result is returned.  In other words, ancillary data points from ATL03 to be included in an ATL06-SR result are treated just like the h_mean, latitude, and longitude variables, and returned as a fitted double-precision floating point value.

2.5.3 ATL06 Subsetted Ancillary Data
####################################

Ancillary data returned from the ``"atl06s"`` and ``"atl06sp"`` APIs come from the land_ice_segments group of the ATL06 granules. The data is mostly returned as-is, with one exception.  Double-precision and single-precision floating point variables are checked to see if they contain the maximum value of their respective encodings, and if so, a floating point NaN (not-a-number) is returned instead.  This check is not performed for integer variables because the maximum value of an encoded integer can sometimes be a valid value (e.g. bit masks).

2.5.4 ATL08-PhoREAL Ancillary Data
##################################

Ancillary data returned from the ``"atl08"`` and ``"atl08p"`` APIs come from the land_segments group of the ATL08 granules.  The data goes through a series of processing steps before being returned back to the user as per-extent (i.e. variable-length segment) result values.

* When a user requests an ATL08 ancillary field, the ATL08 classifications are automatically enabled with all unclassified photons filtered out (i.e. noise, ground, canopy, and top of canopy are included; unclassified photons are excluded).  If the user is also requesting PhoREAL processing, then noise photons are automatically filtered out as well.  Lastly, if the user manually specifies which ATL08 photon classifications to use, then that manual specification takes precedence and is used.
* If a user manually specifies that unclassified photons are to be included, the value used for an ancillary field for that photon has all 1's in the binary encoding of that value.  For example, if it is an 8-bit unsigned integer, the value would be 255.  If it is a double-precision floating point, the value would be -nan.
* Since the ATL08 APIs return per-extent values and not per-photon values, the set of per-photon ancillary field values must be reduced in some way to a single per-extent value to be returned back to the user.  There are currently two options available for how this reduction occurs.

  1. Nearest Neighbor (Mode): the value that appears most often in the extent is selected.  This is the default method.  For example, if a user specifies ``"atl08_fields": ["asr"]`` in their parameters, and the PhoREAL extent consists of mostly two-thirds of one ATL08 segment, and a third of the next ATL08 segment (and if the photons rate is consistent across those two ATL08 segments), then the value returned by the nearest neighbor processing will be the ATL08 land_segment value from the first ATL08 segment.
  2. Interpolation (Average): the value that is the average of all the per-photon values provided.  This is only performed when the user appends a "%" to the end of the field's name in the field list parameter.  For example, if a user specifies ``"atl08_fields": ["asr%"]`` in their parameters, then the returned result is the average of all of the "asr" values in the PhoREAL extent.  Note that the GeoDataFrame returned to the user will include the "%" in the column name for this particular field - that is so a user can request both nearest neighbor and interpolated values for the same field in a single request.


2.6 PhoREAL parameters
-----------------------

The PhoREAL vegetation algorithm, developed at the University of Texas at Austin, provides vegatation statistics over custom-length ATL03 photon segments.  A subset of these algorithms have been integrated into SlideRule are accessed via the _atl08_ and _atl08p_ APIs using the ``"phoreal"`` parameter set.

To enable PhoREAL functionality, the ``"phoreal"`` parameter must be populated in the request dictionary.

* ``"phoreal"``: dictionary of rasters to sample
    - ``"binsize"``: size of the vertical photon bin in meters
    - ``"geoloc"``: algorithm to use to calculate the geolocation (latitude, longitude, along-track distance, and time) of each custom length PhoREAL segment; "mean" - takes the average value across all photons in the segment; "median" - takes the median value across all photons in the segment; "center" - takes the halfway value calculated by the average of the first and last photon in the segment
    - ``"use_abs_h"``: boolean whether the absolute photon heights are used instead of the normalized heights
    - ``"send_waveform"``: boolean whether to send to the client the photon height histograms in addition to the vegetation statistics
    - ``"above_classifier"``: boolean whether to use the ABoVE photon classifier when determining top of canopy photons


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
     - -1 (not considered)
   * - ``"cnt"``
     - Integer
     - 10
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


3.1 Segmented Photon Data (ATL03)
---------------------------------

The photon data is stored as along-track segments inside the ATL03 granules, which is then broken apart by SlideRule and re-segmented according to processing
parameters supplied at the time of the request. The new segments are called **extents**.  When the length of an extent is 40 meters, and the step size is 20 meters, the extent matches the ATL06 segments.

Most of the time, the photon extents are kept internal to SlideRule and not returned to the user.  But there are some APIs that do return raw photon extents for the user to process on their own.
Even though this offloads processing on the server, the API calls can take longer since more data needs to be returned to the user, which can bottleneck over the network.

Photon extents are returned as GeoDataFrames where each row is a photon.  Each extent represents the data that the ATL06 algorithm uses to generate a single ATL06 elevation.
When the step size is shorter than the length of the extent, the extents returned overlap each other which means that each photon is being returned multiple times and will be duplicated in the resulting GeoDataFrame.

The GeoDataFrame for each photon extent has the following columns:

- ``"track"``: reference pair track number (1, 2, 3)
- ``"sc_orient"``: spacecraft orientation (0: backwards, 1: forwards)
- ``"rgt"``: reference ground track
- ``"cycle"``: cycle
- ``"segment_id"``: segment ID of first ATL03 segment in result
- ``"segment_dist"``: along track distance from the equator to the center of the extent (in meters)
- ``"count"``: the number of photons in the segment
- ``"time"``: nanoseconds from Unix epoch (January 1, 1970) without leap seconds
- ``"latitude"``: latitude (-90.0 to 90.0)
- ``"longitude"``: longitude (-180.0 to 180.0)
- ``"x_atc"``: along track distance of the photon in meters (with respect to the center of the segment)
- ``"y_atc"``: across track distance of the photon in meters
- ``"across"``: across track distance of the photon in meters
- ``"height"``: height of the photon in meters
- ``"solar_elevation"``: solar elevation from ATL03 at time of measurement, in degrees
- ``"background_rate"``: background photon counts per second
- ``"atl08_class"``: the photon's ATL08 classification (0: noise, 1: ground, 2: canopy, 3: top of canopy, 4: unclassified)
- ``"atl03_cnf"``: the photon's ATL03 confidence level (-2: TEP, -1: not considered, 0: background, 1: within 10m, 2: low, 3: medium, 4: high)
- ``"quality_ph"``: the photon's quality classification (0: nominal, 1: possible after pulse, 2: possible impulse responpse effect, 3: possible tep)
- ``"yapc_score"``: the photon's YAPC classification (0 - 255, the larger the number the higher the confidence in surface reflection)

Note: when PhoREAL is enabled, the ATL03 extent records (_atl03rec_) are enhanced to include the following populated fields:

- ``"relief"``: ATL08 normalized photon heights
- ``"landcover"``: ATL08 landcover flags
- ``"snowcover"``: ATL08 snowcover flags

3.2 Elevations (ATL06)
----------------------

The primary result returned by SlideRule for ICESat-2 ATL06 processing requests is a set of geolocated elevations corresponding to a geolocated ATL03 along-track segment. The elevations are contained in a GeoDataFrame where each row represents a calculated elevation.

The elevation GeoDataFrame has the following columns:

- ``"extent_id"``: unique ID associated with custom ATL03 segment (removed from final GeoDataFrame by default)
- ``"segment_id"``: segment ID of first ATL03 segment in result
- ``"n_fit_photons"``: number of photons used in final calculation
- ``"pflags"``: processing flags (0x1 - spread too short; 0x2 - too few photons; 0x4 - max iterations reached)
- ``"rgt"``: reference ground track
- ``"cycle"``: cycle
- ``"region"``: region of source granule
- ``"spot"``: laser spot 1 to 6
- ``"gt"``: ground track (10: GT1L, 20: GT1R, 30: GT2L, 40: GT2R, 50: GT3L, 60: GT3R)
- ``"x_atc"``: along track distance from the equator in meters
- ``"time"``: nanoseconds from Unix epoch (January 1, 1970) without leap seconds
- ``"lat"``: latitude (-90.0 to 90.0)
- ``"lon"``: longitude (-180.0 to 180.0)
- ``"h_mean"``: elevation in meters from ellipsoid
- ``"dh_fit_dx"``: along-track slope
- ``"y_atc"``: across-track distance
- ``"w_surface_window_final"``: width of the window used to select the final set of photons used in the calculation
- ``"rms_misfit"``: measured error in the linear fit of the surface
- ``"h_sigma"``: error estimate for the least squares fit model

3.3 Vegetation Metrics (ATL08)
------------------------------

The primary result returned by SlideRule for ICESat-2 ATL08 processing requests is a set of geolocated vegetation metrics corresponding to a geolocated ATL03 along-track segment. The metrics are contained in a GeoDataFrame where each row represents a segment.

The vegetation GeoDataFrame has the following columns:

- ``"extent_id"``: unique ID associated with custom ATL03 segment (removed from final GeoDataFrame by default)
- ``"segment_id"``: segment ID of first ATL03 segment in result
- ``"rgt"``: reference ground track
- ``"cycle"``: cycle
- ``"region"``: region of source granule
- ``"spot"``: laser spot 1 to 6
- ``"gt"``: ground track (10: GT1L, 20: GT1R, 30: GT2L, 40: GT2R, 50: GT3L, 60: GT3R)
- ``"ph_count"``: total number of photons used by PhoREAL algorithm for this extent
- ``"gnd_count"``: number of ground photons used by PhoREAL algorithm for this extent
- ``"veg_count"``: number of vegetation (canopy and top of canopy) photons used by PhoREAL algorithm for this extent
- ``"landcover"``: flag indicating if segment includes land surfaces
- ``"snowcover"``: flag indicating if snow is present in the segment
- ``"time"``: nanoseconds from Unix epoch (January 1, 1970) without leap seconds
- ``"lat"``: latitude (-90.0 to 90.0)
- ``"lon"``: longitude (-180.0 to 180.0)
- ``"x_atc"``: along track distance from the equator in meters
- ``"solar_elevation"``: solar elevation from ATL03 at time of measurement, in degrees
- ``"h_te_median"``: median terrain elevation in meters (absolute heights)
- ``"h_max_canopy"``: maximum relief height for canopy photons
- ``"h_min_canopy"``: minimum relief height for canopy photons
- ``"h_mean_canopy"``: average relief height for canopy photons
- ``"h_canopy"``: 98th percentile relief height for canopy photons
- ``"canopy_openness"``: standard deviation of relief height for canopy photons
- ``"canopy_h_metrics"``: relief height at given percentile for canopy photons


4. Callbacks
============
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
============

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
            "res": 20.0
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
