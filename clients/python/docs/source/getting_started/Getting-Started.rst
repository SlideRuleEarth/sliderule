===============
Getting Started
===============

This documentation is intended to explain how to use `SlideRule`, a C++/Lua framework for on-demand data processing, and its accompanying Python client. 
`SlideRule` is a science data processing service that runs in the cloud and responds to REST API calls to process and return science results.
This software was developed to support science applications for `NASA's Ice Cloud and land Elevation Satellite-2 (ICESat-2)`__, but also has the goal of demonstrating a new paradigm for providing science data products to researchers.

.. __: https://icesat-2.gsfc.nasa.gov/

While `SlideRule` can be accessed by any http client (e.g. curl) by making GET and POST requests to the `SlideRule` service,
the python packages in this repository provide higher level access by hiding the GET and POST requests inside python function calls that accept and return basic python variable types (e.g. dictionaries, lists, numbers).
The `SlideRule-Python` repository also provides some examples and high-level plotting programs through the use of `Jupyter Notebooks <./Examples.html>`_.

Basic Usage
###########

`SlideRule` provides a number of APIs which allow a user to process ICESat-2 and HDF5 data. For example, the ``icesat2.atl06p`` Python function makes a request to the ``atl06`` API and returns calculated segment elevations from ATL03 data within a geospatial region.

.. code-block:: bash

    # import (1)
    import pandas as pd
    from sliderule import icesat2

    # region of interest (2)
    grand_mesa = [ {"lon": -108.3435200747503, "lat": 38.89102961045247},
                   {"lon": -107.7677425431139, "lat": 38.90611184543033}, 
                   {"lon": -107.7818591266989, "lat": 39.26613714985466},
                   {"lon": -108.3605610678553, "lat": 39.25086131372244},
                   {"lon": -108.3435200747503, "lat": 38.89102961045247} ]
    
    # initialize (3)
    icesat2.init("http://127.0.0.1", verbose=True, max_errors=3)

    # processing parameters (4)
    parms = {
        "poly": grand_mesa,
        "srt": icesat2.SRT_LAND,
        "cnf": icesat2.CNF_SURFACE_HIGH,
        "len": 40.0,
        "res": 20.0,
        "maxi": 1
    }

    # make request (5)
    rsps = icesat2.atl06p(parms)

    # analyze response (6)
    df = pd.DataFrame(rsps)

This code snippet performs the following functions with respect to SlideRule:

#. Imports the ``icesat2`` module from the SlideRule Python packages  
#. Defines a polygon that captures the Grand Mesa region  
#. Initializes the ``icesat2`` module with the address of the SlideRule server and other configuration parameters  
#. Builds an ``atl06`` API request structure that specifies the region of interest and processing parameters  
#. Issues the request to the SlideRule service which then returns the results in a Python dictionary  
#. Builds a pandas DataFrame from the results for further analysis  

Python API Reference
####################

`SlideRule ICESat-2 Python API <../user_guide/ICESat-2.html>`_:

- ``init``: Initialize the icesat2 package with the URL to the SlideRule service `(ref) <../user_guide/ICESat-2.html#init>`_
- ``cmr``: Query the NASA Common Metadata Repository (CMR) for a list of data within temporal and spatial parameters `(ref) <../user_guide/ICESat-2.html#cmr>`_
- ``atl06``: Perform ATL06-SR processing on a single ATL03 granule and return gridded elevations `(ref) <../user_guide/ICESat-2.html#atl06>`_
- ``atl06p``: Perform ATL06-SR processing in parallel on ATL03 data and return gridded elevations `(ref) <../user_guide/ICESat-2.html#atl06p>`_
- ``h5``: Read a dataset from an HDF5 file and return the values of the dataset `(ref) <../user_guide/ICESat-2.html#h5>`_
- ``toregion``: Convert a GeoJSON formatted polygon into the format accepted by SlideRule `(ref) <../user_guide/ICESat-2.html#toregion>`_

`SlideRule Python API <../user_guide/SlideRule.html>`_:

- ``source``: Perform API call to SlideRule service `(ref) <../user_guide/SlideRule.html#source>`_
- ``set_url``: Configure the URL of the SlideRule service `(ref) <../user_guide/SlideRule.html#set-url>`_
- ``update_available_servers``: Cause local client to update the list of available SlideRule servers `(ref) <../user_guide/SlideRule.html#update-available-servers>`_
- ``set_verbose``: Configure the verbose setting in the SlideRule client `(ref) <../user_guide/SlideRule.html#set-verbose>`_
- ``set_max_errors``: Configure the maximum number of errors a given server can return before being removed from the list `(ref) <../user_guide/SlideRule.html#set-max_errors>`_
- ``gps2utc``: Convert a GPS based time returned from SlideRule into a UTC time `(ref) <../user_guide/SlideRule.html#gps2utc>`_
