# sliderule

A C++/Lua framework for science data processing.


## I. Prerequisites

1. Basic build environment (Ubuntu 20.04):
   * build-essential
   * libreadline-dev
   * liblua5.3-dev

2. CMake (3.13.0 or greater)

3. Lttng (optional, enable with ENABLE_LTTNG_TRACING, see [core.md](packages/core/core.md) for installation instructions)

4. HDF5 (optional, enable with USE_H5_PACKAGE, see [h5.md](packages/h5/h5.md) for installation instructions)

5. Pistache (optional, enable USE_PISTACHE_PACKAGE, see [pistache.md](packages/pistache/pistache.md) for installation instructions)


## II. Building with CMake

From the base project directory:
1. `make config`
2. `make`
3. `sudo make install`

This will build the following binaries:
* `sliderule`: console application

And perform the following installations:
* `/usr/local/bin`: applications
* `/usr/local/include/sliderule`: class header files for plugin development
* `/usr/local/etc/sliderule`: plugins, configuration files, and scripts

To change compile options exposed by cmake (e.g. disable plugin compilation):
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

   -DENABLE_H5_REST_VOL=[ON|OFF]       configure H5 package to use REST VOL plugin
                                       default: OFF

   -DENABLE_LTTNG_TRACING=[ON|OFF]     configure use of LTTng tracking
                                       default: OFF

   -DUSE_H5_PACKAGE=[ON|OFF]           hdf5 reading/writing
                                       default: ON

   -DUSE_PISTACHE_PACKAGE=[ON|OFF]     http server and client
                                       default: ON

   -DUSE_ICESAT2_PLUGIN=[ON|OFF]       ICESat-2 science data processing
                                       default: ON

   -DUSE_SIGVIEW_PLUGIN=[ON|OFF]       ATLAS raw telemetry processing
                                       default: ON
```


## III. Quick Start

After building and installing sliderule, here are some quick steps you can take to get up and running.

### Self Test
```bash
$ sliderule scripts/apps/test_runner.lua
```

### Server
```bash
$ sliderule scripts/apps/server.lua
```

## IV. Directory Structure

### platforms

Contains the C++ modules that implement an operating system abstraction layer which enables the framework to run on various platforms.

### packages

Contains the C++ modules that implement the primary functions provided by the framework.  The [core](packages/core/core.md) package contains the fundamental framework classes and is not dependent on any other package.  Other packages should only be dependent on the core package or provide conditional compilation blocks that allow the package to be compiled in the absence of any package outside the core package.

By convention, each package contains two files that are named identical to the package directory name: _{package}.cpp_, _{package}.h_.  The _CMakeLists.txt_ provides the object modules and any package specific definitions needed to compile the package.  It also defines the package's globally defined name used in conditional compilation blocks.  The _{package}.cpp_ file provides an initialization function named with the prototype `void init{package}(void)` that is used to initialize the package on startup.  The _{package}.h_ file exports the initialization function and anything else necessary to use the package.

Any target that includes the package should only include the package's h file, and make a call to the package initialization function.

### plugins

In order to build a plugin for sliderule the plugin code must compile down to a shared object that exposes a single function defined as `void init{plugin}(void)` where _{plugin}_ is the name of the plugin.  Note that if developing the plugin in C++ the initialization function must be externed as C in order to prevent the mangling of the exported symbol.

Once the shared library is built, the build system copies the shared object into the sliderule configuration directory (specified by `RUNTIMEDIR` option in the CMakeLists.txt file) with the name _{plugin}.so_.  On startup, the _sliderule_ application reads the _plugins.conf_ file in the configuration directory and loads all plugins listed in that file.  So depending on the target being built, the corresponding source file for the _plugins.conf_ file must also be modified to include the name of the plugin.

### scripts

Contains Lua and Python scripts that can be used for tests and higher level implementations of functionality.

### targets

Contains the source files to make the various executable targets. By convention, targets are named as follows: {application}-{platform}.


## V. Delivering the Code

Run [RELEASE.sh](RELEASE.sh) to create a tarball that can be distributed: `./RELEASE.sh X.Y.Z`

The three number version identifier X.Y.Z has the following convention: Incrementing X indicates an interface change and does not guarantee the preservation of backward compatibility.  Incrementing Y indicates additional or modified functionality that maintains compile-time compatibility but may change a run-time behavior.  Incrementing Z indicates a bug fix or code cleanup and maintains both compile-time and run-time compatibility.

Using a released version of the code, the following two Makefile targets can be used to build easily distributable packages:
1. On Ubuntu systems: `make package`
   * will build a debian package `build/sliderule-X.Y.Z.deb`
   * which can be distributed and then installed via `sudo dpkg -i sliderule-X.Y.Z.deb`
2. On systems with Docker installed: `make docker-image`
   * will build a docker image `sliderule-linux`
   * which can be run via `docker run -it --rm --name sliderule1 sliderule-linux`


## VI. Licensing

Sliderule is licensed under the Apache License, Version 2.0
to the University of Washington under one or more contributor
license agreements.  See the NOTICE file distributed with this
work for additional information regarding copyright ownership.

You may obtain a copy of the License at: http://www.apache.org/licenses/LICENSE-2.0

The following sliderule software components include code sourced from and/or
based off of third party software that has been released as open source to the
public under various open source agreements:
* `packages/core/LuaEngine.cpp`: partial code sourced from https://www.lua.org/
* `scripts/utils/json.lua`: code sourced from https://github.com/rxi/json.lua.git
* `scripts.utils/csv.lua`: code sourced from http://lua-users.org/wiki/CsvUtils
