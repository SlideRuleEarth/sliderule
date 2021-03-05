# h5lite-python

A python module for reading H5 data

## Prerequisites

#### 1. pybind11

```bash
$ sudo pip install "pybind11[global]"
```

Note: make sure you are aware of where you are running `pip` from and which python environment it is associated with.  The cmake files associated with this project assume that pybind11 is installed into the system folder `/usr/local`; on an Ubuntu system it may be necessary to replace `pip` with `usr/bin/pip3` in order to guarantee a system installation.

#### 2. AWS S3 Library Dependencies

When the library is compiled for use with S3, the Python environment in which the module is imported must have the `zlib` and `openssl` libraries installed. When using the system Python, on Ubuntu run:

```bash
$ sudo apt install libcurl4-openssl libssl zlib1g
```

Conda support is currently broken due to openssl not having the necessary version of libcrypto.  As a best effort, run:

```bash
$ conda install zlib openssl
```
