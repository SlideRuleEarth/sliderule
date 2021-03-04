# h5lite-python

A python module for reading H5 data

### Prerequisites

* pybind11

```bash
$ sudo pip install "pybind11[global]"
```

Note: make sure you are aware of where you are running `pip` from and which python environment it is associated with.  The cmake files associated with this project assume that pybind11 is installed into the system folder `/usr/local`; on an Ubuntu system it may be necessary to replace `pip` with `usr/bin/pip3` in order to guarantee a system installation.

