======================
GEDI Plugin Module
======================


1. Overview
===========

The GEDI API currently provides subsetting and raster sampling capabilities to SlideRule for the L1B, L2A, L3, L4A, and L4B datasets.
* The ``L1B`` dataset can be subsetted with waveforms returned for each footprint inside a user-supplied area of interest
* The ``L2A`` dataset can be subsetted with elevations returned for each footprint inside a user-supplied area of interest
* The ``L3`` dataset can be sampled at specific coordinates and associated with any other SlideRule generated data product that is geolocated
* The ``L4A`` dataset can be subsetted with elevation and above-ground vegetation density returned for each footprint inside a user-supplied area of interest
* The ``L4B`` dataset can be sampled at specific coordinates and associated with any other SlideRule generated data product that is geolocated


2. Parameters
=============

The GEDI module provides additional parameters specific to making GEDI processing requests.

* ``"beam"``: specifies which beam to process (0, 1, 2, 3, 5, 6, 8, 11; -1 for all)
* ``"degrade_flag"``: filters footprints based on degrade flag, leaving blank means it is unfiltered
* ``"l2_quality_flag"``: filters footprints based on L2 quality flag, leaving blank means it is unfiltered
* ``"l4_quality_flag"``: filters footprints based on L4 quality flag, leaving blank means it is unfiltered
* ``"surface_flag"``: filters footprints based on surface flag, leaving blank means it is unfiltered

2.1 Parameter reference table
------------------------------

The default set of parameters used by SlideRule are set to recommended settings.

.. list-table:: IGEDI Request Parameters
   :widths: 25 25 50
   :header-rows: 1

   * - Parameter
     - Units
     - Default
   * - ``"beam"``
     - Integer
     - ALL_BEAMS (-1)
   * - ``"degrade_flag"``
     - Boolean
     -
   * - ``"l2_quality_flag"``
     - Boolean
     -
   * - ``"l4_quality_flag"``
     - Boolean
     -
   * - ``"surface_flag"``
     - Boolean
     -


3. Returned data
=========================

The main kind of data returned by the GEDI APIs are elevation and vegetation measurements organized around the concept of a ``footprint``.  An footprint is a single laser shot reflection on the earth from one of GEDI's laser beams and the resulting digitization and measurements made from it.

Each footprint is uniquely identified by a shot number.  The shot number is provided in the underlying source datasets and is consistent from request to request. This means subsequent runs of SlideRule with the same request parameters will return the same shot numbers.


3.1 L1B Footprints
--------------------------

The waveform data is stored as sequential arrays inside the GEDI granules. The data is read by SlideRule, organized into the individual footprints, subsetted to the area of interest specified by the user, and returned as a GeoDataFrame where each row is a footprint and the waveform is a numpy array in the waveform column.

- ``"shot_number"``: unique footprint identifier
- ``"time_ns"``: UNIX timestamp, used as the index for the DataFrame
- ``"latitude"``: latitude (-90.0 to 90.0)
- ``"longitude"``: longitude (-180.0 to 180.0)
- ``"elevation_start"``: elevation in meters of the first bin of the waveform
- ``"elevation_stop"``: elevation in meters of the last bin of the waveform
- ``"solar_elevation"``: solar elevation at time of measurement, in degrees
- ``"beam"``: beam number
- ``"flags"``: flags set for footprint (0x01: degrade, 0x02: l2 quality, 0x04: l4 quality, 0x80: surface)
- ``"tx_size"``: number of samples in tx waveform
- ``"rx_size"``: number of samples in rx waveform
- ``"tx_waveform"``: tx waveform samples
- ``"rx_waveform"``: rx waveform samples

3.2 L2A Footprints
--------------------------

The footprint data is stored along-track inside the GEDI granules. The data is read by SlideRule, organized into the individual footprints, subsetted to the area of interest specified by the user, and returned as a GeoDataFrame where each row is a footprint.

- ``"shot_number"``: unique footprint identifier
- ``"time_ns"``: UNIX timestamp, used as the index for the DataFrame
- ``"latitude"``: latitude (-90.0 to 90.0)
- ``"longitude"``: longitude (-180.0 to 180.0)
- ``"elevation_lowestmode"``: elevation in meters of reflection closest to the surface of the earth
- ``"elevation_highestreturn"``: elevation in meters of reflection farthest from the surface of the earth
- ``"solar_elevation"``: solar elevation at time of measurement, in degrees
- ``"beam"``: beam number
- ``"flags"``: flags set for footprint (0x01: degrade, 0x02: l2 quality, 0x04: l4 quality, 0x80: surface)

3.3 L3 Raster
--------------------------

The following raster datasets are available to sample:

* ``"gedil3-elevation"``: GEDI03_elev_lowestmode_mean_2019108_2022019_002_03.tif
* ``"gedil3-canopy"``: GEDI03_rh100_mean_2019108_2022019_002_03.tif
* ``"gedil3-elevation-stddev"``: GEDI03_elev_lowestmode_stddev_2019108_2022019_002_03.tif
* ``"gedil3-canopy-stddev"``: GEDI03_rh100_stddev_2019108_2022019_002_03.tif
* ``"gedil3-counts"``: GEDI03_counts_2019108_2022019_002_03.tif

For example, if you wanted to sample the GEDI L3 Canopy raster and calculate zonal statistics for every ICESat-2 PhoREAL data point, then you could add the following entry to your parameters for your PhoREAL request:

    .. code-block:: python

        parms["samples"]: {"canopy": {"asset": "gedil3-canopy", "radius": 10.0, "zonal_stats": True}}


3.4 L4A Footprints
--------------------------

The footprint data is stored along-track inside the GEDI granules. The data is read by SlideRule, organized into the individual footprints, subsetted to the area of interest specified by the user, and returned as a GeoDataFrame where each row is a footprint.

- ``"shot_number"``: unique footprint identifier
- ``"time_ns"``: UNIX timestamp, used as the index for the DataFrame
- ``"latitude"``: latitude (-90.0 to 90.0)
- ``"longitude"``: longitude (-180.0 to 180.0)
- ``"elevation"``: elevation in meters of the surface of the earth
- ``"agbd"``: above ground biodensity
- ``"solar_elevation"``: solar elevation at time of measurement, in degrees
- ``"beam"``: beam number
- ``"flags"``: flags set for footprint (0x01: degrade, 0x02: l2 quality, 0x04: l4 quality, 0x80: surface)

3.3 L4B Raster
--------------------------

The following raster datasets are available to sample:

* ``"gedil4b"``: GEDI04_B_MW019MW138_02_002_05_R01000M_V2.tif

For example, if you wanted to sample the GEDI L4B biodensity raster and calculate zonal statistics for every ICESat-2 PhoREAL data point, then you could add the following entry to your parameters for your PhoREAL request:

    .. code-block:: python

        parms["samples"]: {"agdb": {"asset": "gedil4b", "radius": 10.0, "zonal_stats": True}}


4. Callbacks
=============
For large processing requests, it is possible that the data returned from the API is too large or impractical to fit in the local memory of the Python interpreter making the request.
In these cases, certain APIs in the SlideRule Python client allow the calling application to provide a callback function that is called for every result that is returned by the servers.
If a callback is supplied, the API will not return back to the calling application anything associated with the supplied record types, but assumes the callback fully handles processing the data.
The callback function takes the following form:

.. py:function:: callback (record)

    Callback that handles the results of a processing request for the given record.

    :param dict record: the record object, usually a dictionary containing data

Here is an example of a callback being used for the ``gedi01bp`` function:

    .. code-block:: python

        rec_cnt = 0

        def gedi01bp_cb(rec):
            global rec_cnt
            rec_cnt += 1
            print("{} {}".format(rec_cnt, rec["shot_number"]), end='\r')

        gdf = gedi.gedi01bp({}, callbacks = {"gedi01brec": gedi01bp_cb})
