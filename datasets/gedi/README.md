# SlideRule GEDI Plugin

This SlideRule plugin includes modules for subsetting and sampling the GEDI data products.

If you are a science user interested in processing GEDI data with SlideRule, please see the [sliderule-python examples](https://github.com/SlideRuleEarth/sliderule-python) or our [documentation](https://slideruleearth.io) for help getting started.

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

Then build and install the GEDI plugin:
```bash
$ mkdir build
$ cd build
$ cmake <path_to_sliderule_repo>/plugins/gedi -DCMAKE_BUILD_TYPE=Release
$ make
$ make install
```

## III. What Is Provided

The plugin supplies the following endpoints:
* [gedi01b](endpoints/gedi01b.lua): subset GEDI L1B waveforms
* [gedi02a](endpoints/gedi02a.lua): subset GEDI L2A footprints
* [gedi04a](endpoints/gedi04a.lua): subset GEDI L4A footprints

This plugin supplies the following record types:
* `gedil1brec`: a waveform of GEDI data
* `gedil2arec`: a footprint of GEDI data
* `gedil4arec`: a footprint of GEDI data

## IV. Licensing

SlideRule is licensed under the 3-clause BSD license found in the LICENSE file at the root of this source tree.
