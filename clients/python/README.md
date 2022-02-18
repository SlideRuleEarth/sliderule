# sliderule-python
[![Tests](https://github.com/ICESat2-SlideRule/sliderule-python/actions/workflows/test.yml/badge.svg)](https://github.com/ICESat2-SlideRule/sliderule-python/actions/workflows/test.yml)
[![Binder](https://mybinder.org/badge_logo.svg)](https://gke.mybinder.org/v2/gh/ICESat2-SlideRule/sliderule-python/main?urlpath=lab)
[![badge](https://img.shields.io/static/v1.svg?logo=Jupyter&label=PangeoBinderAWS&message=us-west-2&color=orange)](https://aws-uswest2-binder.pangeo.io/v2/gh/ICESat2-SlideRule/sliderule-python/main?urlpath=lab)
[![DOI](https://zenodo.org/badge/311384982.svg)](https://zenodo.org/badge/latestdoi/311384982)

SlideRule's Python client makes it easier to interact with SlideRule from a Python script.

Detailed [documentation](http://icesat2sliderule.org/rtd/) on installing and using this client can be found at [icesat2sliderule.org](http://icesat2sliderule.org).

## I. Installing the SlideRule Python Client

```bash
conda install -c conda-forge sliderule
```

For alternate methods to install SlideRule, including options for developers, please see the [installation instructions](http://icesat2sliderule.org/rtd/getting_started/Install.html) on icesat2sliderule.org.

### Dependencies

Basic functionality of sliderule-python depends on `requests` and `numpy`.  But if you intend on running the example notebooks, please refer to the package requirements listed in `environment.yml` for a full list of recommended python libraries.

## II. Getting Started Using SlideRule

SlideRule is a C++/Lua framework for on-demand data processing. It is a science data processing service that runs in the cloud and responds to REST API calls to process and return science results.

While SlideRule can be accessed by any http client (e.g. curl) by making GET and POST requests to the SlideRule service, the python packages in this repository provide higher level access by hiding the GET and POST requests inside python function calls that accept and return python variable types.

Example usage:
```python
# import
from sliderule import icesat2

# initialize
icesat2.init("icesat2sliderule.org", verbose=True)

# region of interest
region = [ {"lon":-105.82971551223244, "lat": 39.81983728534918},
           {"lon":-105.30742121965137, "lat": 39.81983728534918},
           {"lon":-105.30742121965137, "lat": 40.164048017973755},
           {"lon":-105.82971551223244, "lat": 40.164048017973755},
           {"lon":-105.82971551223244, "lat": 39.81983728534918} ]

# request parameters
parms = {
    "poly": region,
    "srt": icesat2.SRT_LAND,
    "cnf": icesat2.CNF_SURFACE_HIGH,
    "len": 40.0,
    "res": 20.0,
    "maxi": 1
}

# make request
rsps = icesat2.atl06p(parms, "nsidc-s3")
```

More extensive examples in the form of Jupyter Notebooks can be found in the [examples](examples/) folder.

## III. Reference and User's Guide

Please see our [documentation](http://icesat2sliderule.org/rtd/) page for reference and user's guide material.

## IV. Licensing

SlideRule is licensed under the 3-clause BSD license found in the LICENSE file at the root of this source tree.

The following sliderule-python software components include code sourced from and/or based off of third party software
that is distributed under various open source licenses. The appropriate copyright notices are included in the
corresponding source files.
* `sliderule/icesat2.py`: subsetting code sourced from NSIDC download script (Regents of the University of Colorado)
