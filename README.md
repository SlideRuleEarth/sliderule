# sliderule <img src="https://slideruleearth.io/web/assets/images/SlideRule-whiteBelly.png" alt="SlideRule Logo" style="float:right; margin: 0 15px 15px 0; width:50px;">
[![DOI](https://zenodo.org/badge/261318746.svg)](https://zenodo.org/badge/latestdoi/261318746)
[![Python Tests](https://github.com/SlideRuleEarth/sliderule/actions/workflows/pytest.yml/badge.svg?event=workflow_run)](https://github.com/SlideRuleEarth/sliderule/actions/workflows/pytest.yml)
[![Self Tests](https://github.com/SlideRuleEarth/sliderule/actions/workflows/self_test.yml/badge.svg)](https://github.com/SlideRuleEarth/sliderule/actions/workflows/self_test.yml)

#### A cloud-native framework for on-demand science data processing, hosted at [slideruleearth.io](https://slideruleearth.io).

 This repository is for SlideRule developers and contains the source code for the SlideRule server, clients, and supporting services like the documentation website. If you are a science data user interested in using SlideRule, you can get started right away with our [web client](https://client.slideruleearth.io) or check out our [website](https://slideruleearth.io) where you will find [installation instructions](https://slideruleearth.io/rtd/getting_started/Install.html) for our Python client, and [contact information](https://slideruleearth.io/web/contact/) for reaching out to us.


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

3. For dependencies associated with a specific package, see the package readme at `packages/{package}/README.md` for additional installation instructions.

4. For debug builds, there are additional tools that are needed for static analysis:

   ```bash
   $ sudo apt install clang clang-tidy cppcheck
   ```

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
* `/usr/local/etc/sliderule`: configuration files and scripts

To take full control of the compile options exposed by cmake, run the following commands in the root directory (or from anywhere as long as you point to the `CMakeLists.txt` file in the root directory):
1.	`mkdir -p build`
2.	`cd build; cmake <options> ..`
3. `make`
4.  `sudo make install`

For help and all available targets type `make help`.

Options include:
```
   -DCMAKE_BUILD_TYPE=[Release|Debug]  release (optimized both size and speed); or debug (symbols, no optimization, static analysis)
                                       default: release

   -DINSTALLDIR=[prefix]               location to install sliderule
                                       default: /usr/local

   -DSHARED_LIBRARY=[ON|OFF]           build sliderule as a shared library (overrides all other targets)
                                       default: OFF

   -DSKIP_STATIC_ANALYSIS=[ON|OFF]     disable static analysis when in debug configuration to save build time
                                       default: OFF

   -DENABLE_ADDRESS_SANITIZER=[ON|OFF] instrument code with for detecting memory errors
                                       default: OFF

   -DENABLE_CODE_COVERAGE=[ON|OFF]     instrument code for reporting code coverage
                                       default: OFF

   -DENABLE_TIME_HEARTBEAT=[ON|OFF]    replace system gettime calls with 1KHz timer (useful for reducing CPU load when timestamping lots of events)
                                       default: OFF

   -DENABLE_CUSTOM_ALLOCATOR=[ON|OFF]  override new and delete operators for debugging memory leaks
                                       default: OFF

   -DENABLE_H5CORO_ATTRIBUTE_SUPPORT=[ON|OFF] enable reading attribute fields of H5 files (performance penalty)
                                       default: OFF

   -DENABLE_GDAL_ERROR_REPORTING=[ON|OFF] logs GDAL errors to SlideRule log
                                       default: OFF

   -DENABLE_TRACING=[ON|OFF]           compile in trace points
                                       default: OFF

   -DENABLE_TERMINAL=[ON|OFF]          compiles in print2term function calls
                                       default: ON

   -DMAX_FREE_STACK_SIZE               number of freed messages in a message queue before garbage collection run on queue
                                       default: 4096

   -DH5CORO_THREAD_POOL_SIZE           number of threads used to make HDF5 read requests
                                       default: 100

   -DH5CORO_MAXIMUM_NAME_SIZE          maximum number of characters in full path to HDF5 variable
                                       default: 104

```

## III. Quick Start

Here are some steps you can take to setup a basic development environment and get up and running.  More detailed instructions for setting up a full development environment can be found in our [How Tos](https://slideruleearth.io/web/rtd/developer_guide/how_tos/how_tos.html) section of our documentation.  The instructions provided here give an overview of the steps needed.

### 1. Install the base packages needed to build SlideRule

The SlideRule framework is divided up into packages (which are compile-time modules) and plugins (which are run-time modules).  This repository only contains packages; plugins are built from other repositories and are not necessary for core functionality.  The ___core___ package provides the base functionality of SlideRule and must be compiled.  All other packages extend the functionality of SlideRule and are conditionally compiled.

### 2. Install all dependencies

SlideRule uses a relatively recent version of CMake in order to take advantage of some of the later improvements to the tool.  If using Ubuntu 20.04 or later, then the system package is sufficient.

If additional packages are needed, navigate to the package's readme (the README.md file found in each package directory) for instructions on how to build and configure that package. The CMakeLists.txt file present in each package directory will also contain a list of dependencies called out at the top of the file. Once the proper dependencies are installed, the corresponding option must be passed to cmake to ensure the package is built.

For example, if the AWS package was needed, the installation instructions at `packages/aws/README.md` need to be followed, and then the `-DUSE_AWS_PACKAGE=ON` needs to be passed to cmake when configuring the build.

### 3. Build and install SlideRule

The provided Makefile has targets for typical configurations.  See the [II. Building with CMake](#ii-building-with-cmake) section for more details.

### 4. Running SlideRule as a stand-alone application

SlideRule is normally run wuth a lua script passed to it at startup in order to configure which components are run. "Using" SlideRule means developing the lua scripts needed to configure and instantiate the needed SlideRule components. There are a handful of stock lua scripts provided in the repository that can be used as a starting point for developing a project-specific lua script.

A server running at port 9081 can be started via:
```bash
$ sliderule targets/slideruleearth-aws/server.lua <config.json>
```

A self-test that dynamically checks which packages are present and runs their associated unit tests can be started via:
```bash
$ sliderule targets/slideruleearth-aws/test_runner.lua
```

Alternatively, SlideRule can be run as an interactive Lua interpreter.  Just type `sliderule` to start and `ctrl-c` to exit.  The interactive Lua environment is the same enviroment present to the scripts.

## IV. Directory Structure

This section details the directory structure of the SlideRule repository to help you navigate where different functionality resides.

### clients

Contains the source code for the different clients that make interacting with SlideRule easier.  These clients often support additional functionality to aid science data investigations.  See https://slideruleearth.io/rtd/ for more details.

### datasets

Contains packages specific to an earth science dataset or mission.  Datasets are implemented exactly like packages (see [packages](#packages)), yet are separated out into their own parent directory for emphasis. By convention, they are allowed to depend on any package in the *package* directory, but cannot depend on any other *dataset*.

### docs

Contains the source files to build the documentation website hosted at https://slideruleearth.io.

### platforms

Contains the C++ modules that implement an operating system abstraction layer which enables the framework to run on various platforms.

### packages

Contains the C++ modules that implement the primary functions provided by the framework.  See [package list](packages/README.md) for a list of available packages. The [core](packages/core/README.md) package contains the fundamental framework classes and is not dependent on any other package.  Other packages should only be dependent on the core package or provide conditional compilation blocks that allow the package to be compiled in the absence of any package outside the core package.

By convention, each package contains two files that are named identical to the package directory name: _{package}.cpp_, _{package}.h_.  The _CMakeLists.txt_ provides the object modules and any package specific definitions needed to compile the package.  It also defines the package's globally defined name used in conditional compilation blocks.  The _{package}.cpp_ file provides an initialization function named with the prototype `void init{package}(void)` that is used to initialize the package on startup.  The _{package}.h_ file exports the initialization function and anything else necessary to use the package.

Any target that includes the package should only include the package's header file, and make a call to the package initialization function.

### targets

Contains the source files to make the various executable targets. By convention, targets are named as follows: {application}-{platform}.

## V. Plugins

Contains a project or mission specific extension to the SlideRule framework that is loaded at run-time.

In order to build a plugin for SlideRule, the plugin code must compile down to a shared object that exposes a single function defined as `void init{plugin}(void)` where _{plugin}_ is the name of the plugin.  Note that if developing the plugin in C++ the initialization function must be externed as C in order to prevent the mangling of the exported symbol.

Once the shared object is built, the build system must copy the shared object into the SlideRule plugin directory (specified by the `PLUGINDIR` option in the CMakeLists.txt file) with the name _{plugin}.so_.  On startup, the _sliderule_ application scans the configuration directory and loads all plugins present.


## VI. Versioning

The three number version identifier X.Y.Z has the following convention:
* Incrementing X indicates an interface change and does not guarantee the preservation of backward compatibility.
* Incrementing Y indicates additional or modified functionality that maintains backward compatibility.
* Incrementing Z indicates a bug fix or code cleanup that does not change the interface or intended behavior of the code.


## VII. Licensing

SlideRule is licensed under the 3-clause BSD license found in the LICENSE file at the root of this source tree.

The following SlideRule software components include code sourced from and/or based off of third party software
that is distributed under various open source licenses. The appropriate copyright notices are included in the
corresponding source files.
* `packages/core/LuaEngine.cpp`: partial code sourced from https://www.lua.org/ (MIT license)
* `scripts/extensions/json.lua`: code sourced from https://github.com/rxi/json.lua.git (MIT license)
* `packages/core/MathLib.cpp`: point inclusion code based off of https://wrf.ecse.rpi.edu/Research/Short_Notes/pnpoly.html (BSD-style license)
* `scripts/extensions/base64.lua`: base64 encode/decode code based off of https://github.com/iskolbin/lbase64
* `clients/python/sliderule/icesat2.py`: subsetting code sourced from NSIDC download script (Regents of the University of Colorado)

The following third-party libraries can be linked to by SlideRule:
* __Lua__: https://www.lua.org/ (MIT license)
* __GDAL__: https://gdal.org/ (MIT license)
* __Arrow__: https://arrow.apache.org/ (Apache 2.0 license)
* __RapidJSON__: https://github.com/Tencent/rapidjson (MIT license)
* __curl__: https://curl.se/docs/copyright.html (MIT license derivative - see website for license information)
