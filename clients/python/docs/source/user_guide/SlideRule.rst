====================
SlideRule Python API
====================


- ``atl06``: Perform `ATL06-SR processing <../user_guide/SlideRule.html#atl06>`_ on ATL03 data and returns gridded elevations
- ``h5``: Read a dataset from an `HDF5 file <../user_guide/SlideRule.html#h5>`_ and return the values of the dataset
- ``definition``: Get the `binary record definition  <../user_guide/SlideRule.html#definition>`_ of a record type
- ``time``: `Convert times <../user_guide/SlideRule.html#time>`_ from one format to another




atl06
#####

Performs ATL06-SR processing on ATL03 data and return gridded elevations

``response = sliderule.engine("atl06", request)``

request
-------

.. code-block:: python

    request = {
        "atl03-asset" : # asset: atlas-local, atlas-s3
        "resource": # ATL03 HDF5 filename (required)
        "track": # track number: 1, 2, 3, or do not include for all three tracks
        "parms": {
            "srt": # surface type: 0-land, 1-ocean, 2-sea ice, 3-land ice, 4-inland water (default: 3)
            "cnf": # confidence level for PE selection (default: 4)
            "ats": # minimum along track spread (default: 20.0)
            "cnt": # minimum PE count (default: 10)
            "len": # length of ATL06 segment in meters (default: 40.0)
            "res": # step distance for successive ATL06 segments in meters (default: 20.0)
            "maxi": # maximum iterations, not including initial least-squares-fit selection (default: 1)
            "H_min_win": # minimum height of PE window in meters (default: 3.0)
            "sigma_r_max": # maximum robust dispersion in meters (default: 5.0)
        },
        "timeout": # number of seconds to wait for first response (default: 100)
    }

response
--------

.. code-block:: python

    response = [ elevations_1, elevations_2, ..., elevation_n ]
    evelations_n = [ elevation_n1, elevation_n2, ..., elevation_nm ]
    elevation_nm = {
        "segment_id": # segment ID of first ATL03 segment in result
        "rgt": # reference ground track
        "cycle": # cycle
        "spot": # laser spot 1 to 6
        "delta_time": # seconds from GPS epoch (Jan 6, 1980)
        "lat": # latitude
        "lon": # longitude
        "h_mean": # elevation
        "dh_fit_dx": # along-track slope
        "dh_fit_dy": # across-track slope
    }


h5
##

Reads a dataset from an HDF5 file and return the values of the dataset

``response = sliderule.engine("h5", request)``
``values = icesat2.get_values(response[x]["DATA"], response[x]["DATATYPE"], response[x]["SIZE"])``

request
-------

.. code-block:: python

    request = {
        "asset": # asset: atlas-local, atlas-s3 (required)
        "resource": # ATL03 HDF5 filename (required)
        "dataset": # full path to dataset (required, example: "/gt1r/geolocation/segment_ph_cnt")
        "datatype": # datatype: TEXT, INTEGER, REAL, DYNAMIC (default: DYNAMIC, example: sliderule.datatypes["INTEGER"])
        "id": # user supplied id echoed in the result (default: 0)
    }

response
--------

.. code-block:: python

    response = [ data_1, data_2, ..., data_n ]
    data_n = {
        "id": # echoed data id
        "dataset": # echoed data name
        "datatype": # data type: TEXT, INTEGER, REAL
        "offset": # fragement byte offset of returned data stream
        "size": # size in bytes of fragment
        "data[]": # array of bytes representing dataset values of the specified type
    }


definition
##########

Gets the binary record definition of a record type

``response = sliderule.source("definition", request)``

request
-------

.. code-block:: python

    request = {
        "rectype": # SlideRule record type (required, example: "atl06rec.elevation")
    }

response
--------

.. code-block:: python

    response = {
        "<element_1>": {
            "flags": # processing flags
            "offset": # bit offset from start of record
            "type": # element type id or record type if structure
            "elements": # number of elements in array or 1 if not an array
        },
        ...,
        "<element_x>": {
            ...
        }
    }


time
####

Converts times from one format to another

``response = sliderule.source("time", request)``

request
-------

.. code-block:: python

    request = {
        "time": # time value
        "input": # format of above time value: "NOW", "CDS", "GMT", "GPS"
        "output": # desired format of return value: same as above
    }

response
--------

.. code-block:: python

    response = {
        "time": # time value
        "format": # format of time value: "CDS", "GMT", "GPS"
    }
