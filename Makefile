ROOT = $(shell pwd)
BUILD ?= $(ROOT)/build
STAGE ?= $(ROOT)/stage
SLIDERULE_BUILD = $(BUILD)/sliderule
PGC_BUILD = $(BUILD)/pgc
ATLAS_BUILD = $(BUILD)/atlas
ICESAT2_BUILD = $(BUILD)/icesat2
GEDI_BUILD = $(BUILD)/gedi
LANDSAT_BUILD = $(BUILD)/landsat
USGS3DEP_BUILD = $(BUILD)/usgs3dep

# when using the llvm toolchain to build the source
CLANG_OPT = -DCMAKE_USER_MAKE_RULES_OVERRIDE=$(ROOT)/platforms/linux/ClangOverrides.txt -D_CMAKE_TOOLCHAIN_PREFIX=llvm-

# for a MacOSX host to have this ip command you must install homebrew(see https://brew.sh/) then run 'brew install iproute2mac' on your mac host
MYIP ?= $(shell (ip route get 1 | sed -n 's/^.*src \([0-9.]*\) .*$$/\1/p'))

########################
# Core Framework Targets
########################

default-build: ## default build of sliderule
	make -j4 -C $(SLIDERULE_BUILD)

all: default-build atlas pgc gedi icesat2 landsat ## build everything

config: config-release ## configure make for default build

config-release: prep ## configure make for release version of sliderule binary
	cd $(SLIDERULE_BUILD); cmake -DCMAKE_BUILD_TYPE=Release $(ROOT)

config-debug: prep ## configure make for debug version of sliderule binary
	cd $(SLIDERULE_BUILD); cmake -DCMAKE_BUILD_TYPE=Debug $(ROOT)

DEVCFG  = -DUSE_ARROW_PACKAGE=ON
DEVCFG += -DUSE_AWS_PACKAGE=ON
DEVCFG += -DUSE_CCSDS_PACKAGE=ON
DEVCFG += -DUSE_GEO_PACKAGE=ON
DEVCFG += -DUSE_H5_PACKAGE=ON
DEVCFG += -DUSE_LEGACY_PACKAGE=ON
DEVCFG += -DUSE_NETSVC_PACKAGE=ON
DEVCFG += -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

config-development: prep ## configure make for development version of sliderule binary
	cd $(SLIDERULE_BUILD); cmake -DCMAKE_BUILD_TYPE=Release $(DEVCFG) $(ROOT)

config-development-debug: prep ## configure make for debug version of sliderule binary
	cd $(SLIDERULE_BUILD); cmake -DCMAKE_BUILD_TYPE=Debug $(DEVCFG) -DENABLE_TRACING=ON $(ROOT)

config-all: config-development config-atlas config-pgc config-gedi config-icesat2 config-landsat config-usgs3dep ctags ## configure everything
config-all-debug: config-development-debug config-atlas-debug config-pgc-debug config-gedi-debug config-icesat2-debug config-landsat-debug config-usgs3dep-debug ctags ## configure everything for debug

install: ## install sliderule to system
	make -C $(SLIDERULE_BUILD) install

install-all: install install-atlas install-pgc install-gedi install-icesat2 install-landsat install-usgs3dep ## install everything

uninstall: ## uninstall most recent install of sliderule from system
	xargs rm < $(SLIDERULE_BUILD)/install_manifest.txt

########################
# Python Binding Targets
########################

PYTHONCFG  = -DPYTHON_BINDINGS=ON
PYTHONCFG += -DUSE_H5_PACKAGE=ON
PYTHONCFG += -DUSE_AWS_PACKAGE=ON
PYTHONCFG += -DUSE_LEGACY_PACKAGE=ON
PYTHONCFG += -DUSE_CCSDS_PACKAGE=ON
PYTHONCFG += -DUSE_GEO_PACKAGE=ON
PYTHONCFG += -DUSE_NETSVC_PACKAGE=ON
PYTHONCFG += -DENABLE_H5CORO_ATTRIBUTE_SUPPORT=ON
PYTHONCFG += -DH5CORO_THREAD_POOL_SIZE=0
PYTHONCFG += -DH5CORO_MAXIMUM_NAME_SIZE=192
PYTHONCFG += -DICESAT2_PLUGIN_LIBPATH=/usr/local/etc/sliderule/icesat2.so
PYTHONCFG += -DICESAT2_PLUGIN_INCPATH=/usr/local/include/sliderule/icesat2

config-python: prep ## configure make for python bindings (using system environent)
	cd $(SLIDERULE_BUILD); cmake -DCMAKE_BUILD_TYPE=Release -DENABLE_BEST_EFFORT_CONDA_ENV=ON $(PYTHONCFG) $(ROOT)

config-python-conda: prep ## configure make for python bindings (using conda environment)
	cd $(SLIDERULE_BUILD); cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$(CONDA_PREFIX) $(PYTHONCFG) $(ROOT)

########################
# Shared Library Targets
########################

config-library: prep ## configure make for shared library libsliderule.so
	cd $(SLIDERULE_BUILD); cmake -DCMAKE_BUILD_TYPE=Release -DSHARED_LIBRARY=ON $(ROOT)

#####################
# PGC Plugin Targets
#####################

config-pgc-debug: prep ## configure make for pgc plugin
	cd $(PGC_BUILD); cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON $(ROOT)/plugins/pgc

config-pgc: prep ## configure make for pgc plugin
	cd $(PGC_BUILD); cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON $(ROOT)/plugins/pgc

pgc: ## build pgc plugin
	make -j4 -C $(PGC_BUILD)

install-pgc: ## install pgc plugin to system
	make -C $(PGC_BUILD) install

uninstall-pgc: ## uninstall most recent install of icesat2 plugin from system
	xargs rm < $(PGC_BUILD)/install_manifest.txt

########################
# Atlas Plugin Targets
########################

config-atlas: prep ## configure make for atlas plugin
	cd $(ATLAS_BUILD); cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON $(ROOT)/plugins/atlas

config-atlas-debug: prep ## configure make for atlas plugin
	cd $(ATLAS_BUILD); cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON $(ROOT)/plugins/atlas

atlas: ## build atlas plugin
	make -j4 -C $(ATLAS_BUILD)

install-atlas: ## install altas plugin to system
	make -C $(ATLAS_BUILD) install

uninstall-atlas: ## uninstall most recent install of atlas plugin from system
	xargs rm < $(ATLAS_BUILD)/install_manifest.txt

########################
# Icesat2 Plugin Targets
########################

config-icesat2: prep ## configure make for icesat2 plugin
	cd $(ICESAT2_BUILD); cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON $(ROOT)/plugins/icesat2

config-icesat2-debug: prep ## configure make for icesat2 plugin
	cd $(ICESAT2_BUILD); cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON $(ROOT)/plugins/icesat2

icesat2: ## build icesat2 plugin
	make -j4 -C $(ICESAT2_BUILD)

install-icesat2: ## install icesat2 plugin to system
	make -C $(ICESAT2_BUILD) install

uninstall-icesat2: ## uninstall most recent install of icesat2 plugin from system
	xargs rm < $(ICESAT2_BUILD)/install_manifest.txt

########################
# Gedi Plugin Targets
########################

config-gedi: prep ## configure make for gedi plugin
	cd $(GEDI_BUILD); cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON $(ROOT)/plugins/gedi

config-gedi-debug: prep ## configure make for gedi plugin
	cd $(GEDI_BUILD); cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON $(ROOT)/plugins/gedi

gedi: ## build gedi plugin
	make -j4 -C $(GEDI_BUILD)

install-gedi: ## install gedi plugin to system
	make -C $(GEDI_BUILD) install

uninstall-gedi: ## uninstall most recent install of gedi plugin from system
	xargs rm < $(GEDI_BUILD)/install_manifest.txt

##########################
# Landsat Plugin Targets
##########################

config-landsat-debug: prep ## configure make for landsat plugin
	cd $(LANDSAT_BUILD); cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON $(ROOT)/plugins/landsat

config-landsat: prep ## configure make for landsat plugin
	cd $(LANDSAT_BUILD); cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON $(ROOT)/plugins/landsat

landsat: ## build icesat2 plugin
	make -j4 -C $(LANDSAT_BUILD)

install-landsat: ## install icesat2 plugin to system
	make -C $(LANDSAT_BUILD) install

uninstall-landsat: ## uninstall most recent install of icesat2 plugin from system
	xargs rm < $(LANDSAT_BUILD)/install_manifest.txt

##########################
# usgs3dep Plugin Targets
##########################

config-usgs3dep-debug: prep ## configure make for usgs3dep plugin
	cd $(USGS3DEP_BUILD); cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON $(ROOT)/plugins/usgs3dep

config-usgs3dep: prep ## configure make for usgs3dep plugin
	cd $(USGS3DEP_BUILD); cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON $(ROOT)/plugins/usgs3dep

usgs3dep: ## build usgs3dep plugin
	make -j4 -C $(USGS3DEP_BUILD)

install-usgs3dep: ## install icesat2 plugin to system
	make -C $(USGS3DEP_BUILD) install

uninstall-usgs3dep: ## uninstall most recent install of icesat2 plugin from system
	xargs rm < $(USGS3DEP_BUILD)/install_manifest.txt

########################
# Development Targets
########################

scan: prep ## perform static analysis
	cd $(SLIDERULE_BUILD); export CC=clang; export CXX=clang++; scan-build cmake $(CLANG_OPT) $(DEVCFG) $(ROOT)
	cd $(SLIDERULE_BUILD); scan-build -o scan-results make

asan: prep ## build address sanitizer debug version of sliderule binary
	cd $(SLIDERULE_BUILD); export CC=clang; export CXX=clang++; cmake $(CLANG_OPT) $(DEVCFG) -DCMAKE_BUILD_TYPE=Debug -DENABLE_ADDRESS_SANITIZER=ON $(ROOT)
	cd $(SLIDERULE_BUILD); make

ctags: ## generate ctags
	if [ -d ".clangd/index/" ]; then rm -f .clangd/index/*; fi             ## clear clagnd index (before clangd-11)
	if [ -d ".cache/clangd/index/" ]; then rm -f .cache/clangd/index/*; fi ## clear clagnd index (clangd-11)
	/usr/bin/jq -s 'add' $(SLIDERULE_BUILD)/compile_commands.json $(PGC_BUILD)/compile_commands.json $(ICESAT2_BUILD)/compile_commands.json $(GEDI_BUILD)/compile_commands.json $(LANDSAT_BUILD)/compile_commands.json $(USGS3DEP_BUILD)/compile_commands.json > compile_commands.json

testmem: ## run memory test on sliderule
	valgrind --leak-check=full --track-origins=yes --track-fds=yes sliderule $(testcase)

testcpu: ## run cpu test on sliderule
	valgrind --tool=callgrind sliderule $(testcase)
	# kcachegrind callgrind.out.<pid>

testheap: ## run heap test on sliderule
	valgrind --tool=massif --time-unit=B --pages-as-heap=yes sliderule $(testcase)
	# ms_print massif.out.<pid>

testheaptrack: ## analyze results of heap test
	# heaptrack sliderule $(testcase)
	# heaptrack_gui heaptrack.sliderule.<pid>.gz

testcov: ## analyze results of test coverage report
	lcov -c --directory $(SLIDERULE_BUILD) --output-file $(SLIDERULE_BUILD)/coverage.info
	genhtml $(SLIDERULE_BUILD)/coverage.info --output-directory $(SLIDERULE_BUILD)/coverage_html
	# firefox $(SLIDERULE_BUILD)/coverage_html/index.html

testpy: ## run python binding test
	cp scripts/systests/coro.py $(SLIDERULE_BUILD)
	cd $(SLIDERULE_BUILD); /usr/bin/python3 coro.py

########################
# Global Targets
########################

prep: ## create necessary build directories
	mkdir -p $(SLIDERULE_BUILD)
	mkdir -p $(PGC_BUILD)
	mkdir -p $(ATLAS_BUILD)
	mkdir -p $(ICESAT2_BUILD)
	mkdir -p $(GEDI_BUILD)
	mkdir -p $(LANDSAT_BUILD)
	mkdir -p $(USGS3DEP_BUILD)

clean: ## clean last build
	- make -C $(SLIDERULE_BUILD) clean
	- make -C $(PGC_BUILD) clean
	- make -C $(ATLAS_BUILD) clean
	- make -C $(ICESAT2_BUILD) clean
	- make -C $(GEDI_BUILD) clean
	- make -C $(LANDSAT_BUILD) clean
	- make -C $(USGS3DEP_BUILD) clean

distclean: ## fully remove all non-version controlled files and directories
	- rm -Rf $(BUILD)
	- rm -Rf $(STAGE)
	- cd $(ROOT)/docs && ./clean.sh
	- cd $(ROOT)/clients/python && ./clean.sh
	- find -name ".cookies" -exec rm {} \;
	- rm compile_commands.json

help: ## that's me!
	@printf "\033[37m%-30s\033[0m %s\n" "#-----------------------------------------------------------------------------------------"
	@printf "\033[37m%-30s\033[0m %s\n" "# Makefile Help                                                                          |"
	@printf "\033[37m%-30s\033[0m %s\n" "#-----------------------------------------------------------------------------------------"
	@printf "\033[37m%-30s\033[0m %s\n" "#-target-----------------------description------------------------------------------------"
	@grep -E '^[a-zA-Z_-].+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'

