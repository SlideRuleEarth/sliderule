# sliderule-python

SlideRule's Python client makes it easier to interact with SlideRule from a Python script.

### Getting Started Using SlideRule

SlideRule is a C++/Lua framework for on-demand data processing. It is a science data processing service that runs in the cloud and responds to REST API calls to process and return science results.

SlideRule can be accessed by any http client (e.g. curl) by making GET and POST requests to the SlideRule service.  For the purposes of this document, all requests to SlideRule will originate from a Python script using Python's `requests` module.

### Installing the SlideRule Python Client
```
pip install https://github.com/ICESat2-SlideRule/sliderule-python
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
git clone https://github.com/ICESat2-SlideRule/sliderule-python.git
cd sliderule-python
conda env create -f environment.yml
```

### Licensing

Sliderule-Python is licensed under the Apache License, Version 2.0
to the University of Washington under one or more contributor
license agreements.  See the NOTICE file distributed with this
work for additional information regarding copyright ownership.

You may obtain a copy of the License at: http://www.apache.org/licenses/LICENSE-2.0

The following sliderule-python software components include code sourced from and/or
based off of third party software that has been released as open source to the
public under various open source agreements:
* `sliderule/icesat2.py`: subsetting code sourced from NSIDC download script (Regents of the University of Colorado)
