ROOT = $(shell pwd)
BUILD = $(ROOT)/build

USERCFG ?=

CLANG_VER ?= ""

FULL_CFG := -DENABLE_TRACING=ON
FULL_CFG += -DH5CORO_MAXIMUM_NAME_SIZE=208
FULL_CFG += -DENABLE_ADDRESS_SANITIZER=ON
FULL_CFG += -DUSE_ARROW_PACKAGE=ON
FULL_CFG += -DUSE_AWS_PACKAGE=ON
FULL_CFG += -DUSE_CCSDS_PACKAGE=ON
FULL_CFG += -DUSE_CRE_PACKAGE=ON
FULL_CFG += -DUSE_GEO_PACKAGE=ON
FULL_CFG += -DUSE_H5_PACKAGE=ON
FULL_CFG += -DUSE_HDF_PACKAGE=ON
FULL_CFG += -DUSE_LEGACY_PACKAGE=ON
FULL_CFG += -DUSE_STREAMING_PACKAGE=ON

########################
# Application Targets
########################

all: ## build code
	make -j8 -C $(BUILD)

config: prep ## configure make for release version of sliderule
	cd $(BUILD) && \
	cmake -DCMAKE_BUILD_TYPE=Release -DSHARED_LIBRARY=ON $(USERCFG) $(ROOT)

config-all: prep ## configure make for release version of sliderule
	cd $(BUILD) && \
	cmake -DCMAKE_BUILD_TYPE=Release -DSHARED_LIBRARY=ON $(FULL_CFG) $(ROOT)

config-debug: prep ## configure make for static analysis and debug runs
	cd $(BUILD) && \
	export CC=clang$(CLANG_VER) && export CXX=clang++$(CLANG_VER) && \
	cmake -DCMAKE_BUILD_TYPE=Debug -DSHARED_LIBRARY=ON -DCMAKE_USER_MAKE_RULES_OVERRIDE=$(ROOT)/platforms/linux/ClangOverrides.txt -D_CMAKE_TOOLCHAIN_PREFIX=llvm- $(FULL_CFG) $(USERCFG) $(ROOT)

install: ## install sliderule to system
	make -C $(BUILD) install
	ldconfig

uninstall: ## uninstall most recent install of sliderule from system
	xargs rm < $(BUILD)/install_manifest.txt

prep: ## create necessary build directories
	mkdir -p $(BUILD)

clean: ## clean last build
	- make -C $(BUILD) clean

distclean: ## fully remove all non-version controlled files and directories
	- rm -Rf $(BUILD)

########################
# Development Targets
########################

TEST ?=

testmem: ## run memory test on sliderule
	valgrind --leak-check=full --track-origins=yes --track-fds=yes sliderule $(TEST)

testcpu: ## run cpu test on sliderule
	valgrind --tool=callgrind sliderule $(TEST)
	# kcachegrind callgrind.out.<pid>

testheap: ## run heap test on sliderule
	valgrind --tool=massif --time-unit=B --pages-as-heap=yes sliderule $(TEST)
	# ms_print massif.out.<pid>

testheaptrack: ## analyze results of heap test
	# heaptrack sliderule $(TEST)
	# heaptrack_gui heaptrack.sliderule.<pid>.gz

testcov: ## analyze results of test coverage report
	lcov -c --directory $(BUILD) --output-file $(BUILD)/coverage.info
	genhtml $(BUILD)/coverage.info --output-directory $(BUILD)/coverage_html
	# firefox $(BUILD)/coverage_html/index.html

ctags: ## generate ctags
	if [ -d ".clangd/index/" ]; then rm -f .clangd/index/*; fi             ## clear clagnd index (before clangd-11)
	if [ -d ".cache/clangd/index/" ]; then rm -f .cache/clangd/index/*; fi ## clear clagnd index (clangd-11)
	/usr/bin/jq -s 'add' $(SLIDERULE_BUILD)/compile_commands.json $(PGC_BUILD)/compile_commands.json $(ICESAT2_BUILD)/compile_commands.json $(GEDI_BUILD)/compile_commands.json $(LANDSAT_BUILD)/compile_commands.json $(USGS3DEP_BUILD)/compile_commands.json $(OPENDATA_BUILD)/compile_commands.json $(SWOT_BUILD)/compile_commands.json > compile_commands.json

help: ## that's me!
	@printf "\033[37m%-30s\033[0m %s\n" "#-----------------------------------------------------------------------------------------"
	@printf "\033[37m%-30s\033[0m %s\n" "# Makefile Help                                                                          |"
	@printf "\033[37m%-30s\033[0m %s\n" "#-----------------------------------------------------------------------------------------"
	@printf "\033[37m%-30s\033[0m %s\n" "#-target-----------------------description------------------------------------------------"
	@grep -E '^[a-zA-Z_-].+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'

