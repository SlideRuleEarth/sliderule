# sliderule-client

SlideRule's Python client makes it easier to interact with SlideRule from a Python script.

### Getting Started Using SlideRule

SlideRule is a C++/Lua framework for on-demand data processing. It is a science data processing service that runs in the cloud and responds to REST API calls to process and return science results.

SlideRule can be accessed by any http client (e.g. curl) by making GET and POST requests to the SlideRule service.  For the purposes of this document, all requests to SlideRule will originate from a Python script using Python's `requests` module.

### Installing the SlideRule Python Client
```
pip install https://github.com/ICESat2-SlideRule/sliderule-client
```

basic usage:
```python
import sliderule
# TODO:
# or..
from sliderule import icesat2
```

development install:
```
git clone https://github.com/ICESat2-SlideRule/sliderule-client.git
cd sliderule-client
conda env create -f environment.yml
```
