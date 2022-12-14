ROOT = $(shell pwd)
BUILD = $(ROOT)/build/sliderule
ATLAS_BUILD = $(ROOT)/build/atlas
ICESAT2_BUILD = $(ROOT)/build/icesat2

# when using the llvm toolchain to build the source
CLANG_OPT = -DCMAKE_USER_MAKE_RULES_OVERRIDE=$(ROOT)/platforms/linux/ClangOverrides.txt -D_CMAKE_TOOLCHAIN_PREFIX=llvm-

# for a MacOSX host to have this ip command you must install homebrew(see https://brew.sh/) then run 'brew install iproute2mac' on your mac host
MYIP ?= $(shell (ip route get 1 | sed -n 's/^.*src \([0-9.]*\) .*$$/\1/p'))

########################
# Core Framework Targets
########################

all: default-build

default-build: ## default build of sliderule
	make -j4 -C $(BUILD)

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
DEVCFG += -DENABLE_APACHE_ARROW_10_COMPAT=ON

config-development: prep ## configure make for development version of sliderule binary
	cd $(BUILD); cmake -DCMAKE_BUILD_TYPE=Release $(DEVCFG) $(ROOT)

config-development-debug: prep ## configure make for debug version of sliderule binary
	cd $(BUILD); cmake -DCMAKE_BUILD_TYPE=Debug $(DEVCFG) -DENABLE_TRACING=ON $(ROOT)

install: ## install sliderule to system
	make -C $(BUILD) install

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
PYTHONCFG += -DENABLE_H5CORO_ATTRIBUTE_SUPPORT=ON
PYTHONCFG += -DH5CORO_THREAD_POOL_SIZE=0
PYTHONCFG += -DH5CORO_MAXIMUM_NAME_SIZE=192
PYTHONCFG += -DICESAT2_PLUGIN_LIBPATH=/usr/local/etc/sliderule/icesat2.so
PYTHONCFG += -DICESAT2_PLUGIN_INCPATH=/usr/local/include/sliderule

config-python: prep ## configure make for python bindings
	cd $(BUILD); cmake -DCMAKE_BUILD_TYPE=Release $(PYTHONCFG) $(ROOT)

########################
# Shared Library Targets
########################

config-library: prep ## configure make for shared library libsliderule.so
	cd $(BUILD); cmake -DCMAKE_BUILD_TYPE=Release -DSHARED_LIBRARY=ON $(ROOT)

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
# Development Targets
########################

arcticdem-config-debug: prep ## configure make for arcticdem plugin
	cd $(BUILD); cmake -DCMAKE_BUILD_TYPE=Debug $(ROOT)/plugins/arcticdem

arcticdem-config: prep ## configure make for arcticdem plugin
	cd $(BUILD); cmake -DCMAKE_BUILD_TYPE=Release $(ROOT)/plugins/arcticdem

arcticdem-config-debug: prep ## configure make for arcticdem plugin
	cd $(BUILD); cmake -DCMAKE_BUILD_TYPE=Debug $(ROOT)/plugins/arcticdem

arcticdem-config: prep ## configure make for arcticdem plugin
	cd $(BUILD); cmake -DCMAKE_BUILD_TYPE=Release $(ROOT)/plugins/arcticdem

scan: prep ## perform static analysis
	cd $(BUILD); export CC=clang; export CXX=clang++; scan-build cmake $(CLANG_OPT) $(DEVCFG) $(ROOT)
	cd $(BUILD); scan-build -o scan-results make

asan: prep ## build address sanitizer debug version of sliderule binary
	cd $(BUILD); export CC=clang; export CXX=clang++; cmake $(CLANG_OPT) $(DEVCFG) -DCMAKE_BUILD_TYPE=Debug -DENABLE_ADDRESS_SANITIZER=ON $(ROOT)
	cd $(BUILD); make

ctags: config-development ## generate ctags
	cd $(BUILD); cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON $(ROOT)
	mv -f $(BUILD)/compile_commands.json $(ROOT)/compile_commands.json

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
	mkdir -p $(ATLAS_BUILD)
	mkdir -p $(ICESAT2_BUILD)

clean: ## clean last build
	- make -C $(BUILD) clean
	- make -C $(ATLAS_BUILD) clean
	- make -C $(ICESAT2_BUILD) clean

distclean: ## fully remove all non-version controlled files and directories
	- rm -Rf $(BUILD)
	- rm -Rf $(ATLAS_BUILD)
	- rm -Rf $(ICESAT2_BUILD)

help: ## that's me!
	@printf "\033[37m%-30s\033[0m %s\n" "#-----------------------------------------------------------------------------------------"
	@printf "\033[37m%-30s\033[0m %s\n" "# Makefile Help                                                                          |"
	@printf "\033[37m%-30s\033[0m %s\n" "#-----------------------------------------------------------------------------------------"
	@printf "\033[37m%-30s\033[0m %s\n" "#-target-----------------------description------------------------------------------------"
	@grep -E '^[a-zA-Z_-].+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'

