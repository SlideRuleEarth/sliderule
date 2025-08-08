===============
Getting Started
===============

If you haven't already, install the SlideRule Python Client using the `instructions <Install.html>`_ provided.

`SlideRule` provides a number of services which allow a user to process earth science data in the cloud. For example, the ``sliderule.run`` python function makes a request to a specified SlideRule service and returns a GeoDataFrame.

.. code-block:: bash

    # import (1)
    from sliderule import sliderule

    # area of interest (2)
    grand_mesa = [ {"lon": -108.3435200747503, "lat": 38.89102961045247},
                   {"lon": -107.7677425431139, "lat": 38.90611184543033},
                   {"lon": -107.7818591266989, "lat": 39.26613714985466},
                   {"lon": -108.3605610678553, "lat": 39.25086131372244},
                   {"lon": -108.3435200747503, "lat": 38.89102961045247} ]

    # processing parameters (3)
    parms = {
        "poly": grand_mesa,
        "track": 1,
        "cnf": 0,
        "fit": {}
    }

    # make request (4)
    gdf = sliderule.run("atl03x", parms)


This code snippet performs the following functions:

#. Imports the ``sliderule`` module from the SlideRule Python package
#. Defines a polygon that captures the Grand Mesa region (note: sliderule also supports using geojson, shapefiles, and GeoDataFrames to define an area of interest)
#. Builds an API request parameter structure that specifies the region of interest, a specific track to process, a photon confidence level, and the surface fit algorithm
#. Issues the request to the SlideRule service to perform the specified processing on ATL03 and returns the results in a GeoDataFrame for further analysis

Common Package Modules
########################

In the SlideRule Python Package there are a few modules that are used more often than the others.  Refer to the User's Guide and API Reference for further information.

sliderule
  Core SlideRule services for initialization, configuration, processing requests, private cluster provisioning and access, area of interest processing

icesat2
  ICESat-2 specific services and definitions

gedi
  GEDI specific services and definitions

earthdata
  Interface to CMR and other STAC endpoints with helper functions for returning resources given a set of query parameters


Next Steps
####################

You can checkout the `examples <Examples.html>`_  provided in this guide, or move on to the `tutorials </web/rtd/tutorials/user.html>`_ section below.
