===============
Getting Started
===============

This documentation is intended to explain how to use `SlideRule`, a C++/Lua framework for on-demand data processing.
`SlideRule` is a science data processing service that runs in the cloud and responds to REST API calls to process and return science results.
This software was initially developed with the goal of supporting science applications for `NASA's Ice Cloud and land Elevation Satellite-2 (ICESat-2)`__, and demonstrates a new paradigm of data processing from NASA.

.. __: https://icesat-2.gsfc.nasa.gov/

While `SlideRule` can be accessed by any http client (e.g. curl) by making GET and POST requests to the `SlideRule` service.
The python packages in this repository provide higher level access by hiding the GET and POST requests inside python function calls that accept and return basic python variable types (e.g. dictionaries, lists, numbers).
The `SlideRule` python client also provides some examples and high-level plotting programs through the use of `Jupyter Notebooks <./Examples.html>`_.

Basic Usage
###########

The `SlideRule` ``icesat2.atl06p`` function allows a user to process all available ICESat-2 data globally or within a provided polygon.
`SlideRule` can accept polygons as a string, list or dictionary that contain longitudes and latitudes in counter-clockwise order with the first and last point matching.

.. code-block:: bash

    # import
    import pandas as pd
    from sliderule import icesat2

    # initialize
    icesat2.init("http://127.0.0.1", verbose=True, max_errors=3)

    # make request
    parms = {
        "srt": icesat2.SRT_LAND,
        "cnf": icesat2.CNF_SURFACE_HIGH,
        "len": 40.0,
        "res": 20.0,
        "maxi": 1
    }
    rsps = icesat2.atl06p(parms)

    # analyze response
    df = pd.DataFrame(rsps)

SlideRule atl06p Parameters
---------------------------

- ``"poly"``: polygon defining region of interest
- ``"srt"``: surface type: 0-land, 1-ocean, 2-sea ice, 3-land ice, 4-inland water
- ``"len"``: length of the segment in meters
- ``"res"``: step distance for successive segments in meters
- ``"cnf"``: confidence level for PE selection
- ``"maxi"``: maximum number of iterations, not including initial least-squares-fit selection
- ``"ats"``: minimum required along track spread of photon events in a segment
- ``"cnt"``: minimum number of photon events in a segment
- ``"H_min_win"``: minimum height of fit window in meters
- ``"sigma_r_max"``: maximum robust dispersion in meters
