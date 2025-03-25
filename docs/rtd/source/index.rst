================
SlideRule
================

**Process Earth science datasets in the cloud through REST API calls to SlideRule web services.**

:Latest Version: |LatestRelease|
:GitHub: https://github.com/SlideRuleEarth/sliderule
:Web: https://slideruleearth.io/web/
:PyPi: https://pypi.org/project/sliderule/
:Conda: https://anaconda.org/conda-forge/sliderule
:Node.js: https://www.npmjs.com/package/@sliderule/sliderule

SlideRule is a web service for on-demand science data processing, which provides researchers and other Earth science data systems low-latency access to customized data products using processing parameters supplied at the time of the request. SlideRule runs in AWS us-west-2 and has access to ICESat-2, GEDI, Landsat, ArcticDEM, REMA, and other datasets stored in S3 (see `Assets <user_guide/SlideRule.html#assets>`_ for a full list).

"Using SlideRule" typically means running a Python script you've developed to analyze Earth science data, and in that script calling functions in the **sliderule** Python package to make processing requests to SlideRule web services to perform some of the data intensive parts of your analysis.  Most of the documentation and examples we provide are focused on this use-case.  We do provide other means of interacting with SlideRule (most notably the current demo and future web client), but those features are still under development and documentation for them is sparse.


Where To Begin
--------------

.. panels::
    :card: + intro-card text-center
    :column: col-lg-4 col-md-4 col-sm-6 col-xs-12 p-2
    :img-top-cls: pl-2 pr-2 pt-2 pb-2

    ---
    :img-top: assets/sliderule_web_client.png

    **SlideRule Demo**
    ^^^^^^^^^^^^^^^^^^

    Try out an interactive widgets demo.

    .. link-button:: https://demo.slideruleearth.io
        :text: Run Demo
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

   getting_started/SlideRule.rst
   getting_started/Install.rst
   getting_started/Getting-Started.rst
   getting_started/Examples.rst
   getting_started/Project-Map.rst

.. toctree::
   :hidden:
   :maxdepth: 1
   :caption: Users Guide

   user_guide/Background.rst
   user_guide/SlideRule.rst
   user_guide/ICESat-2.rst
   user_guide/GEDI.rst
   user_guide/H5Coro.md
   user_guide/GeoParquet.md
   user_guide/GeoRaster.md
   user_guide/Private-Clusters.md
   user_guide/Under-the-Hood.rst
   user_guide/NASA-Earthdata.rst
   user_guide/Contributing.rst
   user_guide/License.rst

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

   tutorials/user.rst
   tutorials/developer.rst

.. toctree::
   :hidden:
   :maxdepth: 1
   :caption: Release Notes

   release_notes/release_notes.rst
