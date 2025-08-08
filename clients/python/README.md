# sliderule python client

SlideRule's Python client makes it easier to interact with SlideRule from a Python script.

Detailed [documentation](https://slideruleearth.io/rtd/) on installing and using this client can be found at [slideruleearth.io](https://slideruleearth.io/).

## I. Installing the SlideRule Python Client

```bash
conda install -c conda-forge sliderule
```

Basic functionality of sliderule-python depends on `requests`, `numpy`, and `geopandas`.  See [requirements.txt](requirements.txt) for a full list of the requirements.

## II. Example Usage

```python
# import
from sliderule import icesat2

# initialize
icesat2.init("slideruleearth.io", verbose=False)

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
    "res": 20.0
}

# make request
rsps = icesat2.atl06p(parms, "icesat2")
print(f"{rsps}")
```

More extensive examples in the form of Jupyter Notebooks can be found in the [examples](https://github.com/SlideRuleEarth/sliderule-python/tree/main/examples) folder of the [sliderule-python](https://github.com/SlideRuleEarth/sliderule-python) repository.

## III. Reference and User's Guide

Please see our [documentation](https://slideruleearth.io/rtd/) page for reference and user's guide material.
