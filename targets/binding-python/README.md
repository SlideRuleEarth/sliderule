# binding-python

Python bindings for the sliderule library.

----------------------------------------------------------------------------
## I. Prerequisites

#### 1. pybind11

```bash
$ sudo pip install "pybind11[global]"
```

Note: make sure you are aware of where you are running `pip` from and which python environment it is associated with.  The cmake files associated with this project assume that pybind11 is installed into the system folder `/usr/local`; on an Ubuntu system it may be necessary to replace `pip` with `usr/bin/pip3` in order to guarantee a system installation.

#### 2. AWS S3 Library Dependencies

When the library is compiled for use with S3, the Python environment in which the module is imported must have the `zlib` and `openssl` libraries installed. When using the default system Python, on Ubuntu run:

```bash
$ sudo apt install libcurl4-openssl libssl zlib1g
```

Conda support is currently broken due to openssl not having the necessary version of libcrypto.  As a best effort, run:

```bash
$ conda install zlib openssl
```

----------------------------------------------------------------------------
## II. Building and Importing the Bindings

The Python bindings must be built for each target they are used on, and the resulting .so file must either be accessible from the **PYTHONPATH** or be installed into the current Python environment or copied to the same directory as the Python script that is importing it is run from.

To build the bindings:
1. `make config-python`
2. `make`

The results are found in the `build` directory:
* `srpybin.cpython-*.so` - the Python package

To use the package, from a Python script:
```Python
import srpybin
```
----------------------------------------------------------------------------
## III. pyH5Coro

The pyH5Coro module contained within the SlideRule Python bindings is a Python wrapper to the H5Coro C++ module.  It is used to read H5 files from S3 directly from Python scripts without first downloading the file to a local file system.

#### Creating a H5 File Object

`srpybin.h5coro(resource, format, path, region, endpoint)`

* Creates an H5 file object that can be used to read datasets directly from S3 for that file.

* Parameters
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
