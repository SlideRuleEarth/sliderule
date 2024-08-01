# SlideRule SWOT Plugin

This SlideRule plugin includes modules for subsetting and sampling the SWOT data products.

## I. Prerequisites

The base build environment must be setup as described in the [SlideRule](https://github.com/SlideRuleEarth/sliderule) readme.  Follow the first three steps in the Quick Start section to install the necessary system packages.  See [SlideRule Readme: III. Quick Start](https://github.com/SlideRuleEarth/sliderule/blob/master/README.md#iii-quick-start).

## II. Building

Use the [Makefile](Makefile) to build and install the plugin.

For a development version of SlideRule that supports the plugin and is run locally, first build and install the sliderule server application:
```bash
$ make config
$ make
$ make install
```

Then build and install the SWOT plugin:
```bash
$ mkdir build
$ cd build
$ cmake <path_to_sliderule_repo>/plugins/icesat2 -DCMAKE_BUILD_TYPE=Release
$ make
$ make install
```

## III. What Is Provided

The plugin supplies the following endpoints:
* [swotl2](endpoints/swotl2.lua): subset SWOT L2 data

This plugin supplies the following record types:
* `swotl2rec`: a waveform of GEDI data

## IV. Licensing

SlideRule is licensed under the 3-clause BSD license found in the LICENSE file at the root of this source tree.
