# sliderule-python

SlideRule's Python client makes it easier to interact with SlideRule from a Python script.

## I. Installing the SlideRule Python Client
```
pip install https://github.com/ICESat2-SlideRule/sliderule-python
```

development install:
```
git clone https://github.com/ICESat2-SlideRule/sliderule-python.git
cd sliderule-python
conda env create -f environment.yml
```

## II. Getting Started Using SlideRule

SlideRule is a C++/Lua framework for on-demand data processing. It is a science data processing service that runs in the cloud and responds to REST API calls to process and return science results.

While SlideRule can be accessed by any http client (e.g. curl) by making GET and POST requests to the SlideRule service, the python packages in this repository provide higher level access by hiding the GET and POST requests inside python function calls that accept and return basic python variable types (e.g. dictionaries, lists, numbers).

Example usage:
```python
# import
import pandas as pd
from sliderule import icesat2

# initialize
icesat2.init("127.0.0.1", verbose=True, max_errors=3)

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
```

More extensive examples in the form of Jupyter Notebooks can be found in the [examples](examples/) folder.

## III. Licensing

SlideRule is licensed under the 3-clause BSD license found in the LICENSE file at the root of this source tree.

The following sliderule-python software components include code sourced from and/or based off of third party software 
that is distributed under various open source licenses. The appropriate copyright notices are included in the 
corresponding source files.
* `sliderule/icesat2.py`: subsetting code sourced from NSIDC download script (Regents of the University of Colorado)
