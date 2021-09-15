# sliderule
[![DOI](https://zenodo.org/badge/261318746.svg)](https://zenodo.org/badge/latestdoi/261318746)

A C++/Lua framework for on-demand science data processing.

This repository is for SlideRule developers and contains the source code for the SlideRule server application. The server is intended to be deployed alongside plugins containing project specific algorithms for processing science data.  If you are a developer interested in the ICESat-2 plugin for SlideRule, please see the [sliderule-icesat2 plugin](https://github.com/ICESat2-SlideRule/sliderule-icesat2).

If you are a science user interested in processing ICESat-2 data with SlideRule, please see the [sliderule-python client](https://github.com/ICESat2-SlideRule/sliderule-python).

## I. Prerequisites

1. Basic build environment

   * For Ubuntu 20.04:
   ```bash
   $ sudo apt install build-essential libreadline-dev liblua5.3-dev
   ```

   * For CentOS 7:
   ```bash
   $ sudo yum group install "Development Tools"
   $ sudo yum install readline readline-devel
   $ wget http://www.lua.org/ftp/lua-5.3.6.tar.gz
   $ tar -xvzf lua-5.3.6.tar.gz
   $ cd lua-5.3.6
   $ make linux
   $ sudo make install
   ```

2. CMake (3.13.0 or greater)

3. For dependencies associated with a specific package, see the package readme at `packages/{package}/{package}.md` for additional installation instructions.


## II. Building with CMake

From the root directory of the repository:
1. `make config`
2. `make`
3. `sudo make install`

This will build the following binaries:
* `sliderule`: console application

and perform the following installations:
* `/usr/local/bin`: applications
* `/usr/local/include/sliderule`: class header files for plugin development
* `/usr/local/etc/sliderule`: plugins, configuration files, and scripts

To take full control of the compile options exposed by cmake (e.g. disable plugin compilation), run the following commands in the root directory (or from anywhere as long as you point to the `CMakeLists.txt` file in the root directory):
1.	`mkdir -p build`
2.	`cd build; cmake <options> ..`
3. `make`
4.  `sudo make install`

Options include:
```
   -DINSTALLDIR=[prefix]               location to install sliderule
                                       default: /usr/local

   -DRUNTIMEDIR=[directory]            location for run-time files like plugins, configuration files, and lua scripts
                                       default: /usr/local/etc/sliderule

   -DENABLE_COMPAT=[ON|OFF]            configure build for older tool chains (needed to build on CentOS 7)
                                       default: OFF

   -DENABLE_TRACING=[ON|OFF]           compile in trace points
                                       default: OFF

   -DUSE_CCSDS_PACKAGE=[ON|OFF]        CCSDS command and telemetry packet support
                                       default: ON

   -DUSE_LEGACY_PACKAGE=[ON|OFF]       legacy code that supports older applications
                                       default: ON

   -DUSE_AWS_PACKAGE=[ON|OFF]          AWS S3 access
                                       default: OFF

   -DUSE_H5_PACKAGE=[ON|OFF]           hdf5 reading/writing
                                       default: ON

   -DUSE_PISTACHE_PACKAGE=[ON|OFF]     http server and client
                                       default: OFF

   -DUSE_NETSVC_PACKAGE=[ON|OFF]       network services (e.g. https)
                                       default: OFF

   -DPYTHON_BINDINGS=[ON|OFF]          build python bindings instead of sliderule executable (overrides all other targets)
                                       default: OFF

   -DSHARED_LIBRARY=[ON|OFF]           build sliderule as a shared library (overrides all other targets except for PYTHON_BINDINGS)
                                       default: OFF
```


## III. Quick Start

Here are some quick steps you can take to setup a basic development environment and get up and running.

### 1. Install the base packages needed to build SlideRule

The SlideRule framework is divided up into packages (which are compile-time modules) and plugins (which are run-time modules).  The ___core___ package provides the base functionality of SlideRule and must be compiled.  All other packages and all plugins extend the functionality of SlideRule and are conditionally compiled.

Install the basic packages needed to build the core package
```bash
$ sudo apt install build-essential libreadline-dev liblua5.3-dev
```

Install analysis and utility packages used when developing and testing the code
```bash
$ sudo apt install curl meld cppcheck valgrind kcachegrind clang clang-tools lcov
```

### 2. Install a recent version of CMake (>= 3.13.0)

SlideRule uses a relatively recent version of CMake in order to take advantage of some of the later improvements to the tool.  If using Ubuntu 20.04, then the system package is sufficient.
```bash
$ sudo apt install cmake
```

If using an older version of Ubuntu, or another distribution which has an older version of CMake, then it needs to be manually installed. Navigate to https://cmake.org/download/ and grab the latest stable binary installer for linux and follow the instructions there; or alternatively, use the following commands to get and install cmake version 3.18.1.
```bash
$ cd {Downloads}
$ wget https://github.com/Kitware/CMake/releases/download/v3.18.1/cmake-3.18.1-Linux-x86_64.sh
$ cd /opt
$ sudo sh {Downloads}/cmake-3.18.1-Linux-x86_64.sh # accept license and default install location
$ sudo ln -s cmake-3.18.1-Linux-x86_64 cmake
```

If you followed the instructions above to install a non-system cmake into /opt, then make sure to add it to the PATH environment variable.
```bash
export PATH=$PATH:/opt/cmake/bin
```

### 3. Install all dependencies

If additional packages are needed, navigate to the package's readme (the {package}.md file found in each package directory) for instructions on how to build and configure that package.  Once the proper dependencies are installed, the corresponding option must be passed to cmake to ensure the package is built.

For example, if the AWS package was needed, the installation instructions at `packages/aws/aws.md` need to be followed, and then the `-DUSE_AWS_PACKAGE=ON` needs to be passed to cmake when configuring the build.

### 4. Build and install SlideRule

The provided Makefile has targets for typical configurations.  See the [II. Building with CMake](#ii-building-with-cmake) section for more details.

```bash
$ make config
$ make
$ sudo make install
```

### 5. Running SlideRule as a stand-alone application

SlideRule requires a lua script to be passed to it at startup in order to configure which components are run. "Using" SlideRule means developing the lua scripts needed to configure and instantiate the needed SlideRule components. Two stock lua scripts are provided in the repository and can be used as a starting point for developing a project-specific lua script.

A REST server running at port 9081 can be started via:
```bash
$ sliderule scripts/apps/server.lua <config.json>
```

A self-test that dynamically checks which packages are present and runs their associated unit tests can be started via:
```bash
$ sliderule scripts/apps/test_runner.lua
```

## V. Directory Structure

This section details the directory structure of the SlideRule repository to help you navigate where different functionality resides.

### platforms

Contains the C++ modules that implement an operating system abstraction layer which enables the framework to run on various platforms.

### packages

Contains the C++ modules that implement the primary functions provided by the framework.  The [core](packages/core/core.md) package contains the fundamental framework classes and is not dependent on any other package.  Other packages should only be dependent on the core package or provide conditional compilation blocks that allow the package to be compiled in the absence of any package outside the core package.

By convention, each package contains two files that are named identical to the package directory name: _{package}.cpp_, _{package}.h_.  The _CMakeLists.txt_ provides the object modules and any package specific definitions needed to compile the package.  It also defines the package's globally defined name used in conditional compilation blocks.  The _{package}.cpp_ file provides an initialization function named with the prototype `void init{package}(void)` that is used to initialize the package on startup.  The _{package}.h_ file exports the initialization function and anything else necessary to use the package.

Any target that includes the package should only include the package's h file, and make a call to the package initialization function.

### scripts

Contains Lua and other scripts that can be used for tests and higher level implementations of functionality.

### targets

Contains the source files to make the various executable targets. By convention, targets are named as follows: {application}-{platform}.


## V. Plugins

In order to build a plugin for SlideRule, the plugin code must compile down to a shared object that exposes a single function defined as `void init{plugin}(void)` where _{plugin}_ is the name of the plugin.  Note that if developing the plugin in C++ the initialization function must be externed as C in order to prevent the mangling of the exported symbol.

Once the shared object is built, the build system must copy the shared object into the SlideRule configuration directory (specified by the `RUNTIMEDIR` option in the CMakeLists.txt file) with the name _{plugin}.so_.  On startup, the _sliderule_ application reads the _plugins.conf_ file in the configuration directory and loads all plugins listed in that file.  It is the responsibility of the admin deploying the _sliderule_ application to make sure there is a _plugins.conf_ file present and that it contains the names of the plugins that need to be loaded.

For example, to load only the icesat2.so plugin, the _plugins.conf_ file would contain:
```
icesat2
```


## VI. Delivering the Code

Run [RELEASE.sh](RELEASE.sh) to tag the repository and create a tarball that can be distributed: `./RELEASE.sh X.Y.Z`

The three number version identifier X.Y.Z has the following convention: Incrementing X indicates an interface change and does not guarantee the preservation of backward compatibility.  Incrementing Y indicates additional or modified functionality that maintains compile-time compatibility but may change a run-time behavior.  Incrementing Z indicates a bug fix or code cleanup and maintains both compile-time and run-time compatibility.

Using a released version of the code, the following Makefile target can be used to build an easily distributable Ubuntu package:
   * `make package`
   * will build a debian package `build/sliderule-X.Y.Z.deb`
   * which can be distributed and then installed via `sudo dpkg -i sliderule-X.Y.Z.deb`


## VII. Licensing

SlideRule is licensed under the 3-clause BSD license found in the LICENSE file at the root of this source tree.

The following SlideRule software components include code sourced from and/or based off of third party software
that is distributed under various open source licenses. The appropriate copyright notices are included in the
corresponding source files.
* `packages/core/LuaEngine.cpp`: partial code sourced from https://www.lua.org/ (MIT license)
* `scripts/extensions/json.lua`: code sourced from https://github.com/rxi/json.lua.git (MIT license)
* `scripts/extensions/csv.lua`: code sourced from https://github.com/geoffleyland/lua-csv (MIT license)
* `packages/core/MathLib.cpp`: point inclusion code based off of https://wrf.ecse.rpi.edu/Research/Short_Notes/pnpoly.html (BSD-style license)

The following third-party libraries can be linked to by SlideRule:
* __Lua__: https://www.lua.org/ (MIT license)
* __AWS SDK for C++__: https://aws.amazon.com/sdk-for-cpp/ (Apache 2.0 license)
* __HDF5 Library__: https://www.hdfgroup.org (BSD-style license)
* __HDF5 REST VOL Plugin__: https://www.hdfgroup.org (BSD-style license)

The following third-party libraries are linked to indirectly by libraries used by SlideRule:
* __curl__: https://curl.se/docs/copyright.html (open source license - see website for license information)
* __yajl__: https://github.com/lloyd/yajl (open source license - similar to MIT)
