# sliderule

A distributed Lua framework for science data processing.

## Prerequisites

1. Linux Package List (Ubuntu)
   * build-essential
   * libreadline6-dev
   * cmake (3.13.0 or greater)

2. Lua (>= 5.3)
   * Download the latest source from https://www.lua.org (as of this readme, version 5.3.4 has been tested).
   * Untar/unzip the source and cd into the base directory (e.g. lua-5.3.4)
   * `make CC="g++" MYCFLAGS="-fpermissive" linux`
   * `sudo make install`
   * Note this installs into /usr/local and may supercede a system lua installation

## Building with CMake

From the base project directory:
1. `make config`
2. `make`
3. `sudo make install`

This will build the following binaries:
* `sliderule` - console application

and perform the following installations:
* `/usr/local/bin`: applications
* `/usr/local/include/sliderule`: class header files for plugin development
* `/usr/local/etc/sliderule`: plugins, configuration files, and lua scripts

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
