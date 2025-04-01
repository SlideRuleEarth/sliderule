================
SlideRule |LatestRelease|
================

**Process Earth science datasets in the cloud through API calls to SlideRule web services.**

:GitHub: https://github.com/SlideRuleEarth/sliderule
:Documentation: https://slideruleearth.io/web/
:Web Client: https://client.slideruleearth.io/
:Provisioning System: https://ps.slideruleearth.io/
:PyPi: https://pypi.org/project/sliderule/
:Conda: https://anaconda.org/conda-forge/sliderule
:Node.js: https://www.npmjs.com/package/@sliderule/sliderule

Purpose of this Site
---------------------

This documentation is intended to explain how to use `SlideRule` and its accompanying Python client. SlideRule is a web service for on-demand science data processing, which provides researchers and other Earth science data systems low-latency access to customized data products using processing parameters supplied at the time of the request. SlideRule runs in AWS us-west-2 and has access to ICESat-2, GEDI, Landsat, ArcticDEM, REMA, and a growing list of other datasets stored in S3.

While `SlideRule` can be accessed by any http client (e.g. curl) by making GET and POST requests to the `SlideRule` service,
the python packages in this repository provide higher level access to SlideRule by hiding the GET and POST requests inside python function
calls that accept basic python variable types (e.g. dictionaries, lists, numbers), and returns GeoDataFrames.

"Using SlideRule" typically means running a Python script you've developed to analyze Earth science data, and in that script calling functions in the **sliderule** Python package to make processing requests to SlideRule web services to perform some of the data intensive parts of your analysis.  Most of the documentation and examples we provide are focused on this use-case.  We do provide other means of interacting with SlideRule, most notably the web client at https://client.slideruleearth.io, both those aspects of the project are less documented.

Where To Begin
--------------

.. panels::
    :card: + intro-card text-center
    :column: col-lg-4 col-md-4 col-sm-6 col-xs-12 p-2
    :img-top-cls: pl-2 pr-2 pt-2 pb-2

    ---
    :img-top: assets/sliderule_web_client.png

    **Web Client**
    ^^^^^^^^^^^^^^^^^^

    Try out an interactive web client.

    .. link-button:: https://client.slideruleearth.io
        :text: Go To Client
        :classes: stretched-link btn-outline-primary btn-block

    ---
    :img-top: assets/examples.png

    **Examples**
    ^^^^^^^^^^^^

    Jump right in and learn from examples.

    .. link-button:: getting_started/Examples
        :type: ref
        :text: Examples
        :classes: stretched-link btn-outline-primary btn-block

    ---
    :img-top: assets/getting_started.png

    **Getting Started**
    ^^^^^^^^^^^^^^^^^^^

    Walkthrough what SlideRule can do.

    .. link-button:: getting_started/Install
        :type: ref
        :text: Getting Started
        :classes: stretched-link btn-outline-primary btn-block


Contacting Us
-------------

SlideRule is openly developed on GitHub at https://github.com/SlideRuleEarth.  We welcome all feedback and contributions!  For more details on the different ways to reach out to us, see our `Contact Us </web/contact/>`_ page.


Project Information
-------------------

The SlideRule project is funded by NASA's ICESat-2 program and is led by the University of Washington in collaboration with NASA Goddard Space Flight Center.  The first public release of SlideRule occurred in April 2021.  Since then we've continued to add new services, new algorithms, and new datasets, while also making improvements to our processing architecture.  Looking to the future, we hope to make SlideRule an indispensable component in the analysis of a broad array of Earth Science datasets that help us better understand the planet we call home.


.. toctree::
   :hidden:
   :maxdepth: 1
   :caption: Getting Started

   getting_started/Install.rst
   getting_started/Getting-Started.rst
   getting_started/Examples.rst
   getting_started/Contributing.rst
   getting_started/License.rst

.. toctree::
   :hidden:
   :maxdepth: 1
   :caption: Users Guide

   user_guide/overview.rst
   user_guide/basic_usage.rst
   user_guide/ICESat-2.rst
   user_guide/GEDI.rst
   user_guide/GeoParquet.md
   user_guide/GeoRaster.md
   user_guide/Private-Clusters.md
   user_guide/H5Coro.md

.. toctree::
   :hidden:
   :maxdepth: 1
   :caption: API Reference

   api_reference/sliderule.rst
   api_reference/icesat2.rst
   api_reference/gedi.rst
   api_reference/earthdata.rst
   api_reference/h5.rst
   api_reference/raster.rst
   api_reference/ipxapi.rst

.. toctree::
   :hidden:
   :maxdepth: 1
   :caption: Tutorials

   tutorials/tutorials.rst

.. toctree::
   :hidden:
   :maxdepth: 1
   :caption: Developers Guide

   developer_guide/Project-Map.rst
   developer_guide/Under-the-Hood.rst
   developer_guide/endpoints.rst
   developer_guide/how_tos/how_tos.rst
   developer_guide/articles/articles.rst
   developer_guide/design/design.rst
   developer_guide/release_notes/release_notes.rst

.. toctree::
   :hidden:
   :maxdepth: 1
   :caption: Background

   background/ICESat-2.rst
   background/NASA-Earthdata.rst
