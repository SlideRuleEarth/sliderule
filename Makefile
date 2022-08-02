ROOT = $(shell pwd)
BUILD = $(ROOT)/build

# when using the llvm toolchain to build the source
CLANG_OPT = -DCMAKE_USER_MAKE_RULES_OVERRIDE=$(ROOT)/platforms/linux/ClangOverrides.txt -D_CMAKE_TOOLCHAIN_PREFIX=llvm-

# for a MacOSX host to have this ip command you must install homebrew(see https://brew.sh/) then run 'brew install iproute2mac' on your mac host
MYIP ?= $(shell (ip route get 1 | sed -n 's/^.*src \([0-9.]*\) .*$$/\1/p'))

######################
# Development Targets
######################

all: default-build

default-build: ## default build of sliderule
	make -j4 -C $(BUILD)

config: release-config ## configure make for default build

release-config: prep ## configure make for release version of sliderule binary
	cd $(BUILD); cmake -DCMAKE_BUILD_TYPE=Release $(ROOT)

debug-config: prep ## configure make for release version of sliderule binary
	cd $(BUILD); cmake -DCMAKE_BUILD_TYPE=Debug $(ROOT)

DEVCFG  = -DENABLE_TRACING=ON
DEVCFG += -DUSE_AWS_PACKAGE=ON
DEVCFG += -DUSE_CCSDS_PACKAGE=ON
DEVCFG += -DUSE_GEOTIFF_PACKAGE=ON
DEVCFG += -DUSE_H5_PACKAGE=ON
DEVCFG += -DUSE_LEGACY_PACKAGE=ON
DEVCFG += -DUSE_NETSVC_PACKAGE=ON
DEVCFG += -DUSE_PISTACHE_PACKAGE=ON

development-config: prep ## configure make for debug version of sliderule binary
	cd $(BUILD); cmake -DCMAKE_BUILD_TYPE=Debug $(DEVCFG) $(ROOT)

PYTHONCFG  = -DPYTHON_BINDINGS=ON
PYTHONCFG += -DUSE_H5_PACKAGE=ON
PYTHONCFG += -DUSE_AWS_PACKAGE=ON
PYTHONCFG += -DUSE_LEGACY_PACKAGE=ON
PYTHONCFG += -DUSE_CCSDS_PACKAGE=ON
PYTHONCFG += -DUSE_GEOTIFF_PACKAGE=ON
PYTHONCFG += -DENABLE_H5CORO_ATTRIBUTE_SUPPORT=ON
PYTHONCFG += -DH5CORO_THREAD_POOL_SIZE=0
PYTHONCFG += -DH5CORO_MAXIMUM_NAME_SIZE=192
PYTHONCFG += -DICESAT2_PLUGIN_LIBPATH=/usr/local/etc/sliderule/icesat2.so
PYTHONCFG += -DICESAT2_PLUGIN_INCPATH=/usr/local/include/sliderule

python-config: prep ## configure make for python bindings
	cd $(BUILD); cmake -DCMAKE_BUILD_TYPE=Release $(PYTHONCFG) $(ROOT)

library-config: prep ## configure make for shared library libsliderule.so
	cd $(BUILD); cmake -DCMAKE_BUILD_TYPE=Release $(ROOT)

atlas-config: prep ## configure make for atlas plugin
	cd $(BUILD); cmake -DCMAKE_BUILD_TYPE=Release $(ROOT)/plugins/atlas

icesat2-config: prep ## configure make for icesat2 plugin
	cd $(BUILD); cmake -DCMAKE_BUILD_TYPE=Release $(ROOT)/plugins/icesat2

scan: prep ## perform static analysis
	cd $(BUILD); export CC=clang; export CXX=clang++; scan-build cmake $(CLANG_OPT) $(DEVCFG) $(ROOT)
	cd $(BUILD); scan-build -o scan-results make

asan: prep ## build address sanitizer debug version of sliderule binary
	cd $(BUILD); export CC=clang; export CXX=clang++; cmake $(CLANG_OPT) $(DEVCFG) -DCMAKE_BUILD_TYPE=Debug -DENABLE_ADDRESS_SANITIZER=ON $(ROOT)
	cd $(BUILD); make

install: ## install sliderule to system
	make -C $(BUILD) install

uninstall: ## uninstall most recent install of sliderule from system
	xargs rm < $(BUILD)/install_manifest.txt

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

prep: ## create necessary build directories
	mkdir -p $(BUILD)

clean: ## clean last build
	- make -C $(BUILD) clean

distclean: ## fully remove all non-version controlled files and directories
	- rm -Rf $(BUILD)

help: ## that's me!
	@printf "\033[37m%-30s\033[0m %s\n" "#-----------------------------------------------------------------------------------------"
	@printf "\033[37m%-30s\033[0m %s\n" "# Makefile Help                                                                          |"
	@printf "\033[37m%-30s\033[0m %s\n" "#-----------------------------------------------------------------------------------------"
	@printf "\033[37m%-30s\033[0m %s\n" "#-target-----------------------description------------------------------------------------"
	@grep -E '^[a-zA-Z_-].+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'

