# sliderule

A C++/Lua framework for science data processing.


## Prerequisites

1. Basic build environment (Ubuntu 20.04):
   * build-essential
   * libreadline-dev
   * liblua5.3-dev

2. CMake (3.13.0 or greater)

3. Pistache (needed for pistache plugin, see [pistache.md](plugins/pistahce/pistache.md) for installation instructions)

4. HDF5 (needed for h5 plugin, see [h5.md](plugins/pistahce/h5.md) for installation instructions)


## Building with CMake

From the base project directory:
1. `make config`
2. `make`
3. `sudo make install`

This will build the following binaries:
* `sliderule`: console application

And perform the following installations:
* `/usr/local/bin`: applications
* `/usr/local/include/sliderule`: class header files for plugin development
* `/usr/local/etc/sliderule`: plugins, configuration files, and lua scripts

To set compile options exposed by cmake (e.g. disable plugin compilation):
1.	`mkdir -p build`
2.	`cd build; cmake <options> ..`
3. `make`
4.  `sudo make install`

Options include:
* `-DUSE_H5_PLUGIN=[ON|OFF]`
* `-DUSE_ICESAT2_PLUGIN=[ON|OFF]`
* `-DUSE_PISTACHE_PLUGIN=[ON|OFF]`
* `-DUSE_SIGVIEW_PLUGIN=[ON|OFF]`
* `-DINSTALLDIR=[prefix]`: location to install sliderule; default: /usr/local
* `-DRUNTIMEDIR=[directory]`: runtime location for plugins, configuration files, and lua scripts; default /usr/local/etc/sliderule


## Directory Structure

### platforms

Contains the C++ modules that implement an operating system abstraction layer which enables the framework to run on various platforms.

### packages

Contains the C++ modules that implement the primary functions provided by the framework.  The [core](packages/core/core.md) package contains the fundamental framework classes and is not dependent on any other package.  Other packages should only be dependent on the core package or provide conditional compilation blocks that allow the package to be compiled in the absence of any package outside the core package.

By convention, each package contains two files that are named identical to the package directory name: {package}.cpp, {package}.h.  The CMakeLists.txt provides the object modules and any package specific definitions needed to compile the package.  It also defines the package's globally defined name used in conditional compilation blocks.  The cpp file provides an initialization function named with the prototype `void init{package}(void)` that is used to initialize the package on startup.  The h file exports the initialization function and anything else necessary to use the package.

Any target that includes the package should only include the package's h file, and make a call to the package initialization function.

### plugins

In order to build a plugin for sliderule the plugin code must compile down to a shared object that exposes a single function defined as `void init{package}(void)` where _{package}_ is the name of the plugin.  Note that if developing the plugin in C++ the initialization function must be externed as C in order to prevent the mangling of the exported symbol.

Once the shared library is built, copy the shared object into the sliderule configuration directory (specified by CONFDIR in the makefile, and defaulted to /usr/local/etc/sliderule) with the name {package}.so.  On startup, the _sliderule_ application inspects the configuration directory and loads all plugins.

### scripts

Contains Lua scripts that can be used for tests and higher level implementations of functionality.

### targets

Contains the source files to make the various executable targets. By convention, targets are named as follows: {application}-{platform}.

## Delivering the Code


Run [RELEASE.sh](RELEASE.sh) to create a tarball that can be distributed: `./RELEASE.sh X.Y.Z`

The three number version identifier X.Y.Z has the following convention: Incrementing X indicates an interface change and does not guarantee the preservation of backward compatibility.  Incrementing Y indicates additional or modified functionality that maintains compile-time compatibility but may change a run-time behavior.  Incrementing Z indicates a bug fix or code cleanup and maintains both compile-time and run-time compatibility.

Using a released version of the code, the following two Makefile targets can be used to build easily distributable packages:
1. On Ubuntu systems: `make package`
   * will build a debian package `build/sliderule-X.Y.Z.deb`
   * which can be distributed and then installed via `sudo dpkg -i sliderule-X.Y.Z.deb`
2. On systems with Docker installed: `make docker-image`
   * will build a docker image `sliderule-linux`
   * which can be run via `docker run -it --rm --name sliderule1 sliderule-linux`

## Licensing

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
