ICESat-2 (SlideRule)
====================

The ICESat-2 plugin provides science data processing algorithms for derived ATL06 data products.

## I. Building

1. The base build environment must be setup as described in the SlideRule readme.  Follow the first three steps in the Quick Start section to install the necessary system packages, a recent version of cmake, and Docker.  See [SlideRule Readme: III. Quick Start](https://github.com/ICESat2-SlideRule/sliderule/blob/master/README.md#iii-quick-start).  

2. Optionally install __Lttng__. See [Core Package Overview](https://github.com/ICESat2-SlideRule/sliderule/blob/master/packages/core/core.md) for installation instructions.

3. Install __HDF5__, including the REST-VOL connector. See [Install H5 Library](https://github.com/ICESat2-SlideRule/sliderule/blob/master/packages/h5/h5.md) for installation instructions.

4. Install __AWS SDK__. See [Install AWS SDK Library](https://github.com/ICESat2-SlideRule/sliderule/blob/asset/packages/aws/aws.md) for installation instructions.

5. Use the [Makefile](build/Makefile) to build the software.

For a development version of SlideRule that is run locally:
```bash
$ make config
$ make
$ sudo make install
$ make run-stand-alone
```

For a production version of SlideRule that is run in a docker container:
```bash
$ make docker-image
$ make run-docker
```

## II. Setting Up Python Environment

In order to run the provided python test scripts, python needs to be installed on your system and a python environment needs to be setup and some third-party packages installed.  The installation instructions below assume the use of the Anaconda Python distribution.

### Configuring Python Using Conda

Using the Anaconda distribution, the following steps are needed to setup your environment:
```bash
$ conda create -n icesat2
$ conda activate icesat2
$ conda install requests
$ conda install numpy
$ conda install pandas
$ conda install matplotlib
$ conda install cartopy
```

### Install SlideRule Python Client

Install the SlideRule Python Client from an activated environment:
```bash
$ cd scripts/python
$ python setup.py install
```

## III. Programmatic Access to ICESat-2 Plugin

Any python script can access the APIs provided by the ICESat-2 plugin by importing and using the [sliderule.py](../../scripts/python/sliderule/sliderule.py) module.

This plugin supplies the following record types:
* `atl03rec`: a variable along-track extent of ATL03 photon data
* `atl03rec.photons`: individual ATL03 photons
* `atl06rec`: ATL06 algorithm result record
* `atl06rec.elevation`: individual ATL06 elevations

The plugin supplies the following endpoints:
* [atl06](endpoints/atl06.lua): process ATL03 photon data to produce gridded elevations

## IV. Notes

1. To use the MPI_ICESat2_ATL03.py script provided at https://github.com/tsutterley/read-ICESat-2/blob/master/scripts/MPI_ICESat2_ATL03.py, the following steps must be taken to updated your python environment (using the Anaconda distribution):
    * export PYTHONPATH={path/to/read-ICESat-2}:$PYTHONPATH
    * conda install -c conda-forge "h5py>=2.9=mpi*"
    * conda install scipy
    * conda install scikit-learn