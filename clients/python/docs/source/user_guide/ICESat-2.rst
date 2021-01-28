==================
SlideRule ICESat-2
==================


cmr
###

Query the `NASA Common Metadata Repository (CMR) <https://cmr.earthdata.nasa.gov/search>`_ for a list of data within temporal and spatial parameters

.. code-block:: python

    url_list = icesat2.cmr(polygon=polygon, time_start=None, time_end=None,
        version='003', short_name='ATL03')

Keyword Arguments
-----------------

- `polygon` : polygon defining region of interest as ``str``, ``list`` or ``dict``
- `time_start`: starting time for query in format ``<year>-<month>-<day>T<hour>:<minute>:<second>Z``
- `time_end`: ending time for query in format ``<year>-<month>-<day>T<hour>:<minute>:<second>Z``
- `version`: dataset version as found in the `NASA CMR Directory <https://cmr.earthdata.nasa.gov/search/site/collections/directory/eosdis>`_
- `short_name`: dataset short name as defined in the `NASA CMR Directory <https://cmr.earthdata.nasa.gov/search/site/collections/directory/eosdis>`_

Returns
-------

- ``url_list``: list of files for the dataset fitting the spatial and temporal parameters

atl06
#####

Performs ATL06-SR processing on ATL03 data and returns gridded elevations

.. code-block:: python

    response = icesat2.atl06(parms, resource, asset="atl03-cloud", track=0, as_numpy=False)

Arguments
---------

- `parms`: Parameters for the ATL06-SR processing

    * ``"srt"``: surface type: 0-land, 1-ocean, 2-sea ice, 3-land ice, 4-inland water
    * ``"cnf"``: confidence level for PE selection
    * ``"ats"``: minimum along track spread
    * ``"cnt"``: minimum PE count in segment
    * ``"len"``: length of ATL06 segment in meters
    * ``"res"``: step distance for successive ATL06 segments in meters
    * ``"maxi"``: maximum iterations, not including initial least-squares-fit selection
    * ``"H_min_win"``: minimum height of PE window in meters
    * ``"sigma_r_max"``: maximum robust dispersion in meters
- `resource`: ATL03 HDF5 filename

Keyword Arguments
-----------------

- `asset` : ``"atl03-local"``, ``"atl03-cloud"``, ``"atl03-s3"``
- `track`: reference pair track number (1, 2, 3, or None to include for all three)
- `as_numpy`: return as flattened numpy arrays

Returns
-------

- ``"segment_id"``: segment ID of first ATL03 segment in result
- ``"rgt"``: reference ground track
- ``"cycle"``: cycle
- ``"spot"``: laser spot 1 to 6
- ``"delta_time"``: seconds from GPS epoch (Jan 6, 1980)
- ``"lat"``: latitude
- ``"lon"``: longitude
- ``"h_mean"``: elevation
- ``"dh_fit_dx"``: along-track slope
- ``"dh_fit_dy"``: across-track slope


atl06p
######

Performs ATL06-SR processing in parallel on ATL03 data and returns gridded elevations

.. code-block:: python

    response = icesat2.atl06p(parm, asset="atl03-cloud", track=0,
        as_numpy=False, max_workers=4, block=True)

Arguments
---------

- `parms`: Parameters for the ATL06-SR processing

    * ``"poly"``: polygon defining region of interest
    * ``"srt"``: surface type: 0-land, 1-ocean, 2-sea ice, 3-land ice, 4-inland water
    * ``"cnf"``: confidence level for PE selection
    * ``"ats"``: minimum along track spread
    * ``"cnt"``: minimum PE count in segment
    * ``"len"``: length of ATL06 segment in meters
    * ``"res"``: step distance for successive ATL06 segments in meters
    * ``"maxi"``: maximum iterations, not including initial least-squares-fit selection
    * ``"H_min_win"``: minimum height of PE window in meters
    * ``"sigma_r_max"``: maximum robust dispersion in meters

Keyword Arguments
-----------------

- `asset` : ``"atl03-local"``, ``"atl03-cloud"``, ``"atl03-s3"``
- `track`: reference pair track number (1, 2, 3, or None to include for all three)
- `as_numpy`: return as flattened numpy arrays
- `max_workers`: maximum number of threads in concurrent futures pool
- `block`: wait for results to finish before returning

Returns
-------

- ``"segment_id"``: segment ID of first ATL03 segment in result
- ``"rgt"``: reference ground track
- ``"cycle"``: cycle
- ``"spot"``: laser spot 1 to 6
- ``"delta_time"``: seconds from GPS epoch (Jan 6, 1980)
- ``"lat"``: latitude
- ``"lon"``: longitude
- ``"h_mean"``: elevation
- ``"dh_fit_dx"``: along-track slope
- ``"dh_fit_dy"``: across-track slope

h5
##

Reads a dataset from an HDF5 file and return the values of the dataset

.. code-block:: python

    values = icesat2.h5(dataset, resource, asset="atl03-cloud",
        datatype=sliderule.datatypes["REAL"])

Arguments
---------

- `dataset`: full path to dataset variable (e.g. ``/gt1r/geolocation/segment_ph_cnt``)
- `resource`: ATL03 HDF5 filename

Keyword Arguments
-----------------

- `asset` : ``"atl03-local"``, ``"atl03-cloud"``, ``"atl03-s3"``
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
