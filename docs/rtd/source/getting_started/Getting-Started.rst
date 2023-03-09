===============
Getting Started
===============

This documentation is intended to explain how to use `SlideRule`, an on-demand science data processing service, and its accompanying Python client.
`SlideRule` is hosted at ``slideruleearth.io`` and responds to REST API calls to process and return science results.
The software was originally developed to support science applications for `NASA's Ice Cloud and land Elevation Satellite-2 (ICESat-2)`__,
but has the goal of demonstrating a new paradigm for providing science data products to researchers.
Future enhancements to SlideRule include adding datasets from new missions and including algorithms for different types of science applications.

.. __: https://icesat-2.gsfc.nasa.gov/

While `SlideRule` can be accessed by any http client (e.g. curl) by making GET and POST requests to the `SlideRule` service,
the python packages in this repository provide higher level access to SlideRule by hiding the GET and POST requests inside python function
calls that accept and return basic python variable types (e.g. dictionaries, lists, numbers), and GeoDataFrames.

Installation
############

If you haven't already, install the SlideRule Python Client using the `instructions <Install.html>`_ provided.

Basic Usage
###########

The `SlideRule` service provides a number of services which allow a user to process ICESat-2 and HDF5 data. For example, the ``icesat2.atl06p`` python function makes a request to the **atl06** service and returns calculated segment elevations from ATL03 data within a geospatial region.

.. code-block:: bash

    # import (1)
    from sliderule import icesat2

    # region of interest (2)
    grand_mesa = [ {"lon": -108.3435200747503, "lat": 38.89102961045247},
                   {"lon": -107.7677425431139, "lat": 38.90611184543033},
                   {"lon": -107.7818591266989, "lat": 39.26613714985466},
                   {"lon": -108.3605610678553, "lat": 39.25086131372244},
                   {"lon": -108.3435200747503, "lat": 38.89102961045247} ]

    # initialize (3)
    icesat2.init("slideruleearth.io")

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
    gdf = icesat2.atl06p(parms)


This code snippet performs the following functions with respect to SlideRule:

#. Imports the ``icesat2`` module from the SlideRule Python packages
#. Defines a polygon that captures the Grand Mesa region
#. Initializes the ``icesat2`` module with the address of the SlideRule server and other configuration parameters
#. Builds an ``atl06`` API request structure that specifies the region of interest and processing parameters
#. Issues the request to the SlideRule service and returns the results in a pandas GeoDataFrame for further analysis

Common API Calls
####################

Out of all the API calls available in SlideRule, there are a few common ones that handle most use cases.
The functions associated with these APIs are listed below for convenience.  For a complete reference, please see
the API references.

    .. list-table::
       :header-rows: 1

       * - function
         - description
       * - `init <../api_reference/icesat2.html#init>`_
         - Initialize the icesat2 package with the URL to the SlideRule service
       * - `atl06p <../api_reference/icesat2.html#atl06p>`_
         - Perform ATL06-SR processing in parallel on ATL03 data and return geolocated elevations
       * - `atl03sp <../api_reference/icesat2.html#atl03sp>`_
         - Subset ATL03 granuels in parallel and return the photon data
       * - `h5p <../api_reference/icesat2.html#h5p>`_
         - Read a list of datasets in parallel from an HDF5 file and return the values of the datasets
       * - `toregion <../api_reference/icesat2.html#toregion>`_
         - Convert a GeoJSON formatted polygon or shapefile into the format accepted by SlideRule
       * - `get_version <../api_reference/icesat2.html#get-version>`_
         - Get version information for the running servers and client
       * - `icepyx.atl06p <../api_reference/ipxapi.html#atl06p>`_
         - Uses icepyx region to perform ATL06-SR processing in parallel on ATL03 data and return geolocated elevations
       * - `icepyx.atl03sp <../api_reference/ipxapi.html#atl03sp>`_
         - Uses icepyx region to subset ATL03 granuels in parallel and return the photon data
       * - `source <../api_reference/sliderule.html#source>`_
         - Perform a direct API call to a SlideRule service

Next Steps
####################

You can checkout the `examples <Examples.html>`_  provided in this guide, or move on to the `tutorials </rtd/tutorials/user.html>`_ section below.
