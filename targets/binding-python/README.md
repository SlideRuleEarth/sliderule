# binding-python

Python bindings for the sliderule library.

----------------------------------------------------------------------------
## I. Prerequisites

#### 1. Development Environment

To setup a typical development environment on Ubuntu:
```bash
sudo apt install build-essential ca-certificates pkg-config cmake git
```

#### 2. pybind11

```bash
sudo pip install "pybind11[global]"
```
Note: make sure you are aware of where you are running `pip` from and which python environment it is associated with.  The cmake files associated with this project assume that pybind11 is installed into the system folder `/usr/local`; on an Ubuntu system it may be necessary to replace `pip` with `usr/bin/pip3` in order to guarantee a system installation.

#### 3. Library Dependencies

The following packages are dependencies of the sliderule library and must be installed to build the code; on Ubuntu run:
```bash
sudo apt install libreadline-dev liblua5.3-dev libcurl4-openssl-dev libssl-dev uuid-dev zlib1g-dev libgdal-dev
```

#### 4. RapidJSON

RapidJSON is used to parse json responses from web services.  To build and install:
```bash
git clone https://github.com/Tencent/rapidjson.git
cd rapidjson
mkdir -p build
cd build
cmake ..
make -j8
sudo make install
```

----------------------------------------------------------------------------
## II. Building and Importing the Bindings

The Python bindings must be built for each target they are used on, and the resulting .so file must either be accessible from the **PYTHONPATH** or be installed into the current Python environment or copied to the same directory as the Python script being run.  For example, type: `export PYTHONPATH=/usr/local/lib` to set the Python path to the default installation directory.

To build the bindings:
1. `make config-icesat2`
2. `make icesat2`
3. `sudo make install-icesat2`
4. `make config-python`
5. `make`
6. `sudo make install`

The results are found in the `/usr/local/lib` directory:
* `srpybin.cpython-*.so` - the Python package

To use the package from a Python script:
```Python
import srpybin
```

----------------------------------------------------------------------------
## III. Reference

| Module | Description | Source |
|:------:|:-----------:|:------:|
| [h5coro](#pyh5coro) | Wraps H5Coro C++ module to read H5 files | [pyH5Coro.h](./pyH5Coro.h) |
| [s3cache](#pys3cache) | Initializes S3 cache I/O driver | [pyS3Cache.h](./pyS3Cache.h) |
| [credentials](#pycredentialstore) | Credential manager for assets in S3 that require authentication | [pyCredentialStore.h](./pyCredentialStore.h) |
| [lua](#pylua) | Execute local lua scripts provided by SlideRule | [pyLua.h](./pyLua.h) |
| [logger](#pylogger) | Enable and issue SlideRule log messages | [pyLogger.h](./pyLogger.h) |

### pyH5Coro

The pyH5Coro module contained within the SlideRule Python bindings is a Python wrapper to the H5Coro C++ module.  It is used to read H5 files directly from S3 in a Python script without downloading the file to a local file system.

#### Creating a H5 File Object

`srpybin.h5coro(asset, resource, format, path, region, endpoint)`

* Creates an H5 file object that can be used to read datasets directly from S3 for that file.

* Parameters
  * __asset__: name of the asset (only meaningful if credentials are setup)
  * __resource__: name of the H5 file
  * __format__: type of access
    * 'file': local file access
    * 's3': direct access to S3
    * 's3cache': caches entire file locally from S3 before reading
  * __path__: subfolder path in S3 bucket to get to H5 file
  * __region__: AWS region (e.g. 'us-west-2')
  * __endpoint__: AWS endpoint (e.g. 'https://s3.us-west-2.amazonaws.com')

* Returns an object that can be used to read the H5 file


#### Reading Metadata from a Dataset

`{h5file}.meta(dataset)`

* Reads the metadata from a dataset

* Parameters
  * __dataset__: full path to dataset within H5 file

* Returns a dictionary of metadata for the dataset
  * __elements__: number of elements in the dataset
  * __typesize__: size in bytes of each element
  * __datasize__: size in bytes of entire dataset
  * __datatype__: string providing the data type
  * __numcols__: number of columns in the dataset
  * __numros__: number of rows in the dataset


#### Reading a Dataset

`{h5file}.read(dataset, column, start_row, number_of_rows)`

* Reads a dataset

* Parameters
  * __dataset__: full path to dataset within H5 file
  * __column__: the column of the dataset to read
  * __start_row__: the starting row of the specified column to start the read from
  * __number_of_rows__: the number of rows to read

* Returns a list values


#### Reading a Dataset in Parallel

`{h5file}.readp(datasets)`

* Reads a dataset

* Parameters
  * __datasets__: a list of datasets to read, each specified as a list with the following values:
    * dataset name
    * column
    * start row
    * number of rows

* Returns a dictionary of lists values, where each key in the dictionary is a dataset name and the corresponding list is the values read for that dataset


### pyS3Cache

The pyS3Cache module is used to initialize the S3 cache I/O driver and is only necessary when the "s3cache" format is specified for an H5 read

#### Initializing the S3Cache

`srpybin.s3cache(cache_root, max_files)`

* Initializes the S3 file cache

* Parameters
  * __cache_root__: local file system directory where the cache will be located
  * __max_files__: maximum number of files to hold in the cache at any one time


### pyCredentialStore

The pyCredentialStore module is used to manage AWS credentials.  AWS credentials are associated with an asset, and then if that asset is specified in an H5 read, the credentials are used as a part of the read request to S3.  AWS credentials consist of an "accessKeyId", "secretAccessKey", "sessionToken", and "expiration".

#### Creating a Credential Manager

`srpybin.credentials(asset)`

* Creates a object to store and manage credentials for an asset

* Parameters
  * __asset__: name of the asset that the credentials will be associated with

* Returns an object that can be used to provide and retrieve credentials

#### Providing credentials

`{credentialObj}.provide(credential)`

* Associates the AWS credentials with the asset managed by the credential object

* Parameters
  * __credential__: dictionary containing the AWS credentials
    * accessKeyId
    * secretAccessKey
    * sessionToken
    * expiration

#### Retrieve credentials

`{credentialObj}.retrieve()`

* Returns the AWS credentials for the asset managed by the credential object; only useful for debugging



### pyLogger

The pyLogger module is used to enable and generate SlideRule log messages.  By default, log messages generated inside the SlideRule library are not displayed in the terminal. When a pyLogger object is created, it listens for log messages and displays them to the terminal.  It also provides a mechanism for generating SlideRule log messages instead of simply printing to the terminal.

#### Creating a Logger

`srpybin.logger(level)`

* Enables SlideRule log messages and creates an object that is able to generate SlideRule log messages

* Parameters
  * __level__: minimal criticality level of the log messages to be displayed
    * srpybin.CRITICAL
    * srpybin.ERROR
    * srpybin.WARNING
    * srpybin.INFO
    * srpybin.DEBUG

* Returns an object that can be used to generate new SlideRule log messages

#### Issue Log Message

`srpybin.critical(msg)`

`srpybin.error(msg)`

`srpybin.warning(msg)`

`srpybin.info(msg)`

`srpybin.debug(msg)`

* Generate a SlideRule log message

* Parameters
  * __msg__: the message to be sent

----------------------------------------------------------------------------
## IV. Example

The following example reads three datasets from a GEDI granule residing in the ORNL DAAC's protected S3 bucket in US-West-2.

```Python
# python

import srpybin
import requests

# setup logging
logger = srpybin.logger(srpybin.INFO)

# parameters
asset = "ornldaac"
resource = "GEDI04_A_2019229131935_O03846_02_T03642_02_002_02_V002.h5"
format = "s3"
path = "ornl-cumulus-prod-protected/gedi/GEDI_L4A_AGB_Density_V2_1/data"
region = "us-west-2"
endpoint = "https://s3.us-west-2.amazonaws.com"
start_footprint = 93000
num_footprints = 1000
#            dataset,                     col,       start_row,       num_rows
datasets = [["/BEAM0000/lat_lowestmode",    0, start_footprint, num_footprints],
            ["/BEAM0000/lon_lowestmode",    0, start_footprint, num_footprints],
            ["/BEAM0000/agbd",              0, start_footprint, num_footprints]]

# credentials
earthadata_s3 = "https://data.ornldaac.earthdata.nasa.gov/s3credentials"
rsps = requests.get(earthadata_s3)
rsps.raise_for_status()
s3credentials = rsps.json()
srcredentials = srpybin.credentials(asset)
srcredentials.provide({"accessKeyId":       s3credentials['accessKeyId'],
                      "secretAccessKey":    s3credentials['secretAccessKey'],
                      "sessionToken":       s3credentials['sessionToken'],
                      "expiration":         s3credentials["expiration"]})

# perform read
h5file = srpybin.h5coro(asset, resource, format, path, region, endpoint)
data = h5file.readp(datasets)

# display first 5 elements of agdb
print(data["/BEAM0000/agbd"][:5])
```
