# sliderule

A C++/Lua framework for on-demand science data processing.


## I. Prerequisites

1. Basic build environment (Ubuntu 20.04):
   * build-essential
   * libreadline-dev
   * liblua5.3-dev

2. CMake (3.13.0 or greater)

3. For dependencies associated with a specific package, see the package readme at `packages/{package}/{package}.md` for additional installation instructions.


## II. Building with CMake

From the `targets/sliderule-linux` directory:
1. `make config`
2. `make`
3. `sudo make install`

This will build the following binaries:
* `sliderule`: console application

And perform the following installations:
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

   -DENABLE_H5_REST_VOL=[ON|OFF]       configure H5 package to use REST VOL plugin
                                       default: OFF

   -DENABLE_LTTNG_TRACING=[ON|OFF]     configure use of LTTng tracking (see packages/core/core.md for installation instructions)
                                       default: OFF

   -DUSE_CCSDS_PACKAGE=[ON|OFF]        CCSDS command and telemetry packet support
                                       default: ON

   -DUSE_LEGACY_PACKAGE=[ON|OFF]       legacy code that supports older applications
                                       default: ON

   -DUSE_AWS_PACKAGE=[ON|OFF]          AWS S3 access
                                       default: OFF

   -DUSE_H5_PACKAGE=[ON|OFF]           hdf5 reading/writing
                                       default: OFF

   -DUSE_PISTACHE_PACKAGE=[ON|OFF]     http server and client
                                       default: OFF

   -DUSE_ICESAT2_PLUGIN=[ON|OFF]       ICESat-2 science data processing
                                       default: OFF

   -DUSE_SIGVIEW_PLUGIN=[ON|OFF]       ATLAS raw telemetry processing
                                       default: ON
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

### 3. Installing Docker

The official Docker installation instructions found at https://docs.docker.com/engine/install/ubuntu/.

For Ubuntu 20.04, Docker can be installed with the following commands:
```bash
$ sudo apt install docker.io
```

In order to run docker without having to be root, use the following commands:
```bash
$ sudo usermod -aG docker {username}
$ newgrp docker # apply group to user
```

### 4. Install all dependencies

If additional packages are needed, navigate to the packages readme (the {package}.md file found in each package directory) for instructions on how to build and configure that package.  Once the proper dependencies are installed, the corresponding option must be passed to cmake to ensure the package is built.

For example, if the H5 package was needed, first the installation instructions at `packages/h5/h5.md` need to be followed, and then the `-DUSE_H5_PACKAGE=ON` needs to be passed to cmake when configuring the build.

### 5. Build and install SlideRule

The provided Makefile has targets for typical configurations.  See the [II. Building with CMake](#ii-building-with-cmake) section for more details.

```bash
$ make config
$ make
$ sudo make install
```

### 6. Running SlideRule as a stand-alone application

SlideRule requires a lua script to be passed to it runs in order to establish which components are run. "Using" SlideRule means to develop the lua scripts needed to configure and insgantiate the needed SlideRule components. Two stock lua scripts are provided in the repository and can be used as a starting point for developing a project-specific lua script.

A REST server running at port 9081 can be started via:
```bash
$ sliderule scripts/apps/server.lua <config.json>
```

A self-test that dynamically checks which packages and plugins are present and runs their associated unit tests can be started via:
```bash
$ sliderule scripts/apps/test_runner.lua
```

## IV. Building and Running with Docker

Assuming you have docker installed and configured in your system, the following steps can be taken to build and run a Docker container that executes the SlideRule application.

### 1. Build the development Docker image

The first step is to build a docker container that has all of the build dependencies needed to build SlideRule and all its packages.
```bash
$ make docker-development-image
```

### 2. Run the development Docker container

Run the development docker container and from there build the SlideRule application.  Note that the command provided below assumes that you want to be able to use this container to build a SlideRule container (i.e. Docker in Docker) and therefore maps into the host docker.sock to the running container.
```bash
$ docker run -it --rm --name=sliderule-dev -v /var/run/docker.sock:/var/run/docker.sock -v {abs. path to sliderule}:/sliderule sliderule-development
```

### 7. Run SlideRule application in a Docker container

The command below runs the server application inside the Docker container and can be configured in the following ways:
* A script other than `/usr/local/scripts/apps/server.lua` can be passed to the SlideRule executable running inside the Docker container
* The {config.json} file provided to the server.lua script can be used to change server settings
* Environment variables can be passed via `-e {parameter=value}` on the command line to docker
* Different local files and directories can be mapped in via `-v {source abs. path}:{destination abs. path}` on the command line to docker

```bash
$ docker run -it --rm --name=sliderule-app -v /data:/data -p 9081:9081 sliderule-application /usr/local/scripts/apps/server.lua {config.json}
```


## V. Directory Structure

This section details the directory structure of the SlideRule repository to help you navigate where different functionality resides.

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
* `scripts/extensions/json.lua`: code sourced from https://github.com/rxi/json.lua.git
* `scripts/extensions/csv.lua`: code sourced from https://github.com/geoffleyland/lua-csv
