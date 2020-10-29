# sliderule

A C++/Lua framework for on-demand data processing.

### Getting Started Using SlideRule

SlideRule is an on-demand science data processing service that runs in the cloud and responds to REST API calls to process and return science results.

SlideRule can be accessed by any http client (e.g. curl) by making GET and POST requests to the SlideRule service.  For the purposes of this document, all requests to SlideRule will originate from a Python script using Python's ___requests___ module.

### Installing the SlideRule Python Client

The SlideRule software repository provides a helper module (hereafter known as SlideRule's Python client) that makes it easier to interact with SlideRule from a Python script.

The Python client requires the ___requests___ and ___numpy___ modules to be installed.

From the python environment you wish to install into, run the following:
```bash
$ python setup.py install
```