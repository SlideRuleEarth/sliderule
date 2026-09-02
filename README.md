# <img src="https://client.slideruleearth.io/IceSat-2_SlideRule_logo.png" alt="SlideRule Logo" style="float:left; margin: 0 15px 15px 0; width:50px;"> SlideRule
[![DOI](https://zenodo.org/badge/261318746.svg)](https://zenodo.org/badge/latestdoi/261318746)
[![Test Report](https://github.com/SlideRuleEarth/sliderule/actions/workflows/sliderule-testreport.yml/badge.svg)](https://github.com/SlideRuleEarth/sliderule/actions/workflows/sliderule-testreport.yml)

#### A cloud-native framework for on-demand science data processing, hosted at [slideruleearth.io](https://slideruleearth.io).

This repository is for SlideRule developers and contains the source code for the SlideRule server, clients, and supporting services like the documentation website. If you are a science data user interested in using SlideRule, you can get started right away with our [web client](https://client.slideruleearth.io) or check out our [documentation](https://docs.slideruleearth.io) where you will find [installation instructions](https://docs.slideruleearth.io/getting_started/Install.html) for our Python client.

## Repository Layout

```
apps/node/          # C++ server (the main binary)
  packages/         # Core compile-time packages (core, arrow, aws, geo, h5coro, …)
  datasets/         # Mission-specific packages (icesat2, gedi, swot, …); cannot depend on other datasets
  scripts/          # Lua entry points: server.lua, test_runner.lua, job_runner.lua, openapi.lua
apps/ams/           # Asset Metadata Service (Python/Flask Lambda)
apps/provisioner/   # Cluster provisioner Lambda (Python)
apps/runner/        # Job runner Lambda (Python)
apps/authenticator/ # GitHub OAuth Lambda (Python)
clients/python/     # pip-installable Python client
clients/nodejs/     # npm package @sliderule/sliderule
targets/slideruleearth/  # THE primary Makefile — all developer commands live here
build/sliderule/    # CMake out-of-tree build output
stage/sliderule/    # CMake install destination (staged for Docker)
version.txt         # Single source of version truth (e.g. v5.4.4)
```

## Cluster Node (apps/node)

This is the primary application that performs all of the on-demand science data processing.  It runs in EC2 as a cluster of instances behind the Intelligent Load Balancer (apps/ilb).

### C++ Build

All commands run from `targets/slideruleearth/`:

```bash
make config-debug    # configure with clang + ASan + clang-tidy + cppcheck
make config-release  # configure release build (no static analysis)
make                 # build + install (make -j8 && make install)
make build           # build inside Docker buildenv container
```

**Critical quirks:**
- Debug builds **require clang** — `ClangOverrides.txt` overrides the compiler to `clang`/`clang++`.
- `clang-tidy` runs with `-warnings-as-errors=*` — any clang-tidy warning **breaks the build**.
- `SKIP_STATIC_ANALYSIS=ON` speeds up iteration but **must be OFF before committing**.
- Inject custom CMake flags without editing the Makefile: `make config-debug USERCFG="-DFOO=ON"`.
- Install prefix is `stage/sliderule/`, not `/usr/local`.

### Running the Node Locally

The cluster node requires the `ilb` and the `ams` to be running in the background.
```bash
docker compose up ilb ams -d
```

Execute the cluster node locally for testing
```bash
make run       # starts server.lua on port 9081 with env vars pre-set
make selftest  # runs test_runner.lua — the only truly offline C++ tests
make job       # runs job_runner.lua
```

The Makefile injects required env vars (`LOG_FORMAT`, `IPV4`, `CLUSTER`, `DOMAIN`, `AMS`, etc.) automatically. Pass `RUNNER=valgrind` to wrap the binary.

### Lua Selftests (offline, local)
```bash
make selftest
```
`test_runner.lua` auto-discovers `selftests/*.lua` under all packages and datasets. Tag filter: pass the package name wrapped in `__` (e.g., `__core__`).

### Application Layout

Inside the `apps/node` application there are the following important subdirectories:
*  __datasets__:  Contains packages specific to an earth science dataset or mission.  Datasets are implemented exactly like packages (see [packages](#packages)), yet are separated out into their own parent directory for emphasis. By convention, they are allowed to depend on any package in the *package* directory, but cannot depend on any other *dataset*.
* __platforms__: Contains the C++ modules that implement an operating system abstraction layer which enables the framework to run on various platforms.
* __packages__: Contains the C++ modules that implement the primary functions provided by the framework.  See [package list](packages/README.md) for a list of available packages. The [core](packages/core/README.md) package contains the fundamental framework classes and is not dependent on any other package.  Other packages should only be dependent on the core package or provide conditional compilation blocks that allow the package to be compiled in the absence of any package outside the core package. By convention, each package contains two files that are named identical to the package directory name: _{package}.cpp_, _{package}.h_.  The _CMakeLists.txt_ provides the object modules and any package specific definitions needed to compile the package.  It also defines the package's globally defined name used in conditional compilation blocks.  The _{package}.cpp_ file provides an initialization function named with the prototype `void init{package}(void)` that is used to initialize the package on startup.  The _{package}.h_ file exports the initialization function and anything else necessary to use the package. Any target that includes the package should only include the package's header file, and make a call to the package initialization function.

### Architecture Notes

- **Lua is the scripting layer:** The server binary accepts a Lua script at startup. All configuration, component instantiation, test orchestration, and OpenAPI generation are in Lua.
- **Package init pattern:** Every C++ package exposes `void init{package}(void)` (e.g., `initcore()`, `initgeo()`). Each has a `{package}.cpp` and `{package}.h`.

### Plugins

A plugin contains a project or mission specific extension to the SlideRule framework that is loaded at run-time.

In order to build a plugin for the SlideRule cluster, the plugin code must compile down to a shared object that exposes a single function defined as `void init{plugin}(void)` where _{plugin}_ is the name of the plugin.  Note that if developing the plugin in C++ the initialization function must be externed as C in order to prevent the mangling of the exported symbol.

Once the shared object is built, the build system must copy the shared object into the SlideRule plugin directory (specified by the `CONFDIR` option in the CMakeLists.txt file) with the name _{plugin}.so_.  On startup, the _sliderule_ application scans the configuration directory and loads all plugins present.

## Python System Tests

For the cluster node tests, start the server locally via `make run`, then:

```bash
make sliderule-test
```

For all the microservice tests, they are executed as standalone tests. From the same makefile in `targets/slideruleearth/` that builds the cluster node, run:
```bash
make ams-test [ARGS="-k test_name"]
make authenticator-test [ARGS="-k test_name"]
make provisioner-test [ARGS="-k test_name"]
make runner-test
```
`ARGS` is passed directly to pytest. All run inside their respective conda environments.

### Conda Environments

Each service has its own named conda env. Use `conda run -n <env>` or activate before running tests.

| Service | Conda env |
|---------|-----------|
| Python client | `sliderule` |
| AMS | `ams` |
| Authenticator | `authenticator` |
| Provisioner | `provisioner` |
| Runner | `runner` |
| Documentation | `myst` |

Install Python client into its env: `make python` (from `targets/slideruleearth/`).

## OpenAPI

```bash
make openapi           # bundle + lint all service specs
make sliderule-openapi # just the server
```
The server binary runs `openapi.lua` to generate the spec, then `@redocly/cli` processes it.

## Release

The release target updates `version.txt` and `clients/python/version.txt`, commits, tags, pushes, creates GitHub release, and builds/pushes Docker images.

```bash
make release RELEASE=vX.Y.Z   # from targets/slideruleearth/
```

The three number version identifier X.Y.Z has the following convention:
* Incrementing X indicates an interface change and does not guarantee the preservation of backward compatibility.
* Incrementing Y indicates additional or modified functionality that maintains backward compatibility.
* Incrementing Z indicates a bug fix or code cleanup that does not change the interface or intended behavior of the code.

## Licensing

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
