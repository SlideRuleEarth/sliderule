## Install HDF5 Library

To get the latest release and instructions, go to https://www.hdfgroup.org/downloads/hdf5.

Make sure zlib is installed:
```bash
$ sudo apt install zlib1g-dev
```

Unpack and build the release:
```bash
$ git clone https://github.com/HDFGroup/hdf5.git
$ cd hdf5
$ git checkout 1.12/master
$ mkdir build
$ cd build
$ CFLAGS=-fPIC ../configure --prefix=/usr/local --disable-shared --enable-build-mode=production
$ make
$ sudo make install
```

If using the REST-VOL plugin, the following steps needs to be taken in addition to the build above.  This is so that the REST-VOL can pull in a thread-safe version of the hdf5 library.  Due to the way REST-VOL is built, it still needs the installation above, but the hdf5 library needs to be overwritten with the one below.  This is currently okay because the high-level api built above is not used by REST-VOL but is required at compile time.
```bash
$ cd {hdf5 repo root}
$ mkdir build-rest
$ cd build-rest
$ CFLAGS=-fPIC ../configure --prefix=/usr/local --disable-shared --enable-build-mode=production --enable-threadsafe --disable-hl
$ make
$ sudo make install
```

Note: the compile options above were for the following reasons:
* static linking reduces system dependencies when installing the sliderule executable (e.g. docker)
* thread safety is needed for parallel access to HDF5 files
* position independent code setting is required for the vol-rest compilation below

## Install REST-VOL Plugin

If the `ENABLE_H5_REST_VOL` cmake option is ON, the following steps are needed to build and install the HDF5 REST VOL plugin required by the h5 package.  Note that the HDF5 library must be installed first (see the steps above).

Make sure that the curl library development files are installed on your system:
```bash
$ sudo apt install libcurl4-openssl-dev
```

Download Yet Another JSON Library and install it. (For more information go to https://lloyd.github.io/yajl/. Note that the REST-VOL connetor needs at least version  2.1).
```bash
$ git clone https://github.com/lloyd/yajl.git
$ cd yajl
$ ./configure
$ make
$ sudo make install
```

Download the latest REST-VOL source and switch to the latest branch (which is necessary in order to work with HDF5 1.12 and later).  Note that the repository being used is forked from the HDF group's repository due to local fixes that needed to be made.  The official HDF repository for the REST-VOL connector is: https://github.com/HDFGroup/vol-rest.git.
```bash
$ git clone https://github.com/ICESat2-SlideRule/vol-rest.git
$ cd vol-rest
$ git checkout thread_safe_option # should be hdf5_1_12_update
```

Build and install the REST VOL plugin using the previously built HDF5 library (note that the build script will fail to install to /usr/local, yet it does not run correctly under sudo, so the multi-step process below is recommended, which includes messages that the build failed; but keep going - running the install as sudo fixes it):
```bash
$ ./build_vol_cmake.sh -s -H /usr/local -P /usr/local
$ cd rest_vol_cmake_build_files/
$ sudo make install
```