ICESat-2 (SlideRule)
====================

The ICESat-2 plugin provides science data processing algorithms for derived ATL06 data products.

## I. Building

This plugin requires the `h5` and `pistache` packages to be enabled.

## II. Setting Up Python Environment

The easiest (and recommended) way to interact with sliderule from a python script is to use the Anaconda distribution.  It includes everything needed to run the icesat2 plugin python scripts.  Alternatively, a local python environment can be setup using the steps below.

### Installing and Configuring Python

Install python packages (Ubuntu)
````bash
$ sudo apt install python3
$ sudo apt install python3-pip
$ sudo apt install python3-venv
````

Create your virtual environment to manage the ***icesat2*** plugin's python package dependencies.
````bash
$ cd sliderule # assuming you've checked out the sliderule repository here
$ python3 -m venv .venv # creates the vitual environment in the .venv directory
````

Activate your virtual environment and update and install the necessary packages.
````bash
$ source .venv/bin/activate
$ pip install -U setuptools pip
$ pip install requests
$ pip install numpy
$ pip install pandas
$ pip install bokeh
````

## III. Quick Start for the ICESat-2 Plugin

The first step is to run the sliderule server:
```bash
$ sliderule scripts/apps/server.lua
```

Then use ***icesat2's*** python client for sliderule:
````bash
$ cd sliderule; source .venv/bin/activate # only needed if using a locally configured python environment as detailed above 
$ python plugins/icesat2/apps/iceplot.py
````

To exit from the local python virtual environment, execute the following in the terminal with the activated environment.
````bash
$ deactivate
````

## IV. Programmatic Access to ICESat-2 Plugin

Any python script can access the APIs provided by the ICESat-2 plugin by importing and using the [sliderule.py](../../scripts/extensions/sliderule.py) module.

The plugin supplies the following record types:
* `atl03rec`: a variable along-track extent of ATL03 photon data
* `atl03rec.photons`: individual ATL03 photons
* `atl06rec`: ATL06 algorithm result record
* `atl06rec.elevation`: individual ATL06 elevations

The plugin supplies the following endpoints:
* [atl06](endpoints/atl06.lua): process ATL03 photon data to produce gridded elevations

## V. Notes

1. To use the MPI_ICESat2_ATL03.py script provided at https://github.com/tsutterley/read-ICESat-2/blob/master/scripts/MPI_ICESat2_ATL03.py, the following steps must be taken to setup an python environment (using the Anaconda distribution):
    * export PYTHONPATH={path/to/read-ICESat-2}:$PYTHONPATH
    * conda create -n icesat2
    * conda activate icesat2
    * conda install -c conda-forge "h5py>=2.9=mpi*"
    * conda install scipy
    * conda install scikit-learn