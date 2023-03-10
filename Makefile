ROOT = $(shell pwd)
BUILD = $(ROOT)/build/sliderule
PGC_BUILD = $(ROOT)/build/pgc
ATLAS_BUILD = $(ROOT)/build/atlas
ICESAT2_BUILD = $(ROOT)/build/icesat2
GEDI_BUILD = $(ROOT)/build/gedi
LANDSAT_BUILD = $(ROOT)/build/landsat

# when using the llvm toolchain to build the source
CLANG_OPT = -DCMAKE_USER_MAKE_RULES_OVERRIDE=$(ROOT)/platforms/linux/ClangOverrides.txt -D_CMAKE_TOOLCHAIN_PREFIX=llvm-

# for a MacOSX host to have this ip command you must install homebrew(see https://brew.sh/) then run 'brew install iproute2mac' on your mac host
MYIP ?= $(shell (ip route get 1 | sed -n 's/^.*src \([0-9.]*\) .*$$/\1/p'))

########################
# Core Framework Targets
########################

default-build: ## default build of sliderule
	make -j4 -C $(BUILD)

all: default-build atlas pgc gedi icesat2 landsat ## build everything

config: config-release ## configure make for default build

config-release: prep ## configure make for release version of sliderule binary
	cd $(BUILD); cmake -DCMAKE_BUILD_TYPE=Release $(ROOT)

config-debug: prep ## configure make for release version of sliderule binary
	cd $(BUILD); cmake -DCMAKE_BUILD_TYPE=Debug $(ROOT)

DEVCFG  = -DUSE_ARROW_PACKAGE=ON
DEVCFG += -DUSE_AWS_PACKAGE=ON
DEVCFG += -DUSE_CCSDS_PACKAGE=ON
DEVCFG += -DUSE_GEO_PACKAGE=ON
DEVCFG += -DUSE_H5_PACKAGE=ON
DEVCFG += -DUSE_LEGACY_PACKAGE=ON
DEVCFG += -DUSE_NETSVC_PACKAGE=ON
DEVCFG += -DUSE_PISTACHE_PACKAGE=ON
DEVCFG += -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

config-development: prep ## configure make for development version of sliderule binary
	cd $(BUILD); cmake -DCMAKE_BUILD_TYPE=Release $(DEVCFG) $(ROOT)

config-development-debug: prep ## configure make for debug version of sliderule binary
	cd $(BUILD); cmake -DCMAKE_BUILD_TYPE=Debug $(DEVCFG) -DENABLE_TRACING=ON $(ROOT)

config-development-cicd: prep ## configure make for debug version of sliderule binary
	cd $(BUILD); cmake -DCMAKE_BUILD_TYPE=Debug $(DEVCFG) -DENABLE_APACHE_ARROW_10_COMPAT=ON $(ROOT)

config-all: config-development config-atlas config-pgc config-gedi config-icesat2 config-landsat ## configure everything

install: ## install sliderule to system
	make -C $(BUILD) install

install-all: install install-atlas install-pgc install-gedi install-icesat2 install-landsat ## install everything

uninstall: ## uninstall most recent install of sliderule from system
	xargs rm < $(BUILD)/install_manifest.txt

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
	cd $(BUILD); cmake -DCMAKE_BUILD_TYPE=Release -DENABLE_BEST_EFFORT_CONDA_ENV=ON $(PYTHONCFG) $(ROOT)

config-python-conda: prep ## configure make for python bindings (using conda environment)
	cd $(BUILD); cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$(CONDA_PREFIX) $(PYTHONCFG) $(ROOT)

########################
# Shared Library Targets
########################

config-library: prep ## configure make for shared library libsliderule.so
	cd $(BUILD); cmake -DCMAKE_BUILD_TYPE=Release -DSHARED_LIBRARY=ON $(ROOT)

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
	cd $(ATLAS_BUILD); cmake -DCMAKE_BUILD_TYPE=Release $(ROOT)/plugins/atlas

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
	cd $(ICESAT2_BUILD); cmake -DCMAKE_BUILD_TYPE=Release $(ROOT)/plugins/icesat2

config-icesat2-debug: prep ## configure make for icesat2 plugin
	cd $(ICESAT2_BUILD); cmake -DCMAKE_BUILD_TYPE=Debug $(ROOT)/plugins/icesat2

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
	cd $(GEDI_BUILD); cmake -DCMAKE_BUILD_TYPE=Release $(ROOT)/plugins/gedi

config-gedi-debug: prep ## configure make for gedi plugin
	cd $(GEDI_BUILD); cmake -DCMAKE_BUILD_TYPE=Debug $(ROOT)/plugins/gedi

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

########################
# Development Targets
########################

scan: prep ## perform static analysis
	cd $(BUILD); export CC=clang; export CXX=clang++; scan-build cmake $(CLANG_OPT) $(DEVCFG) $(ROOT)
	cd $(BUILD); scan-build -o scan-results make

asan: prep ## build address sanitizer debug version of sliderule binary
	cd $(BUILD); export CC=clang; export CXX=clang++; cmake $(CLANG_OPT) $(DEVCFG) -DCMAKE_BUILD_TYPE=Debug -DENABLE_ADDRESS_SANITIZER=ON $(ROOT)
	cd $(BUILD); make

ctags: config-pgc config-development ## generate ctags
	if [ -d ".clangd/index/" ]; then rm -f .clangd/index/*; fi             ## clear clagnd index (before clangd-11)
	if [ -d ".cache/clangd/index/" ]; then rm -f .cache/clangd/index/*; fi ## clear clagnd index (clangd-11)
	/usr/bin/jq -s 'add' $(BUILD)/compile_commands.json $(PGC_BUILD)/compile_commands.json  $(LANDSAT_BUILD)/compile_commands.json > compile_commands.json

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
	lcov -c --directory $(BUILD) --output-file $(BUILD)/coverage.info
	genhtml $(BUILD)/coverage.info --output-directory $(BUILD)/coverage_html
	# firefox $(BUILD)/coverage_html/index.html

testpy: ## run python binding test
	cp scripts/systests/coro.py $(BUILD)
	cd $(BUILD); /usr/bin/python3 coro.py

########################
# Global Targets
########################

prep: ## create necessary build directories
	mkdir -p $(BUILD)
	mkdir -p $(PGC_BUILD)
	mkdir -p $(ATLAS_BUILD)
	mkdir -p $(ICESAT2_BUILD)
	mkdir -p $(GEDI_BUILD)
	mkdir -p $(LANDSAT_BUILD)

clean: ## clean last build
	- make -C $(BUILD) clean
	- make -C $(PGC_BUILD) clean
	- make -C $(ATLAS_BUILD) clean
	- make -C $(ICESAT2_BUILD) clean
	- make -C $(GEDI_BUILD) clean
	- make -C $(LANDSAT_BUILD) clean

distclean: ## fully remove all non-version controlled files and directories
	- rm -Rf $(BUILD)
	- rm -Rf $(PGC_BUILD)
	- rm -Rf $(ATLAS_BUILD)
	- rm -Rf $(ICESAT2_BUILD)
	- rm -Rf $(GEDI_BUILD)
	- rm -Rf $(LANDSAT_BUILD)

help: ## that's me!
	@printf "\033[37m%-30s\033[0m %s\n" "#-----------------------------------------------------------------------------------------"
	@printf "\033[37m%-30s\033[0m %s\n" "# Makefile Help                                                                          |"
	@printf "\033[37m%-30s\033[0m %s\n" "#-----------------------------------------------------------------------------------------"
	@printf "\033[37m%-30s\033[0m %s\n" "#-target-----------------------description------------------------------------------------"
	@grep -E '^[a-zA-Z_-].+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'

