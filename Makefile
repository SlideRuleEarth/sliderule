ROOT = $(shell pwd)
BUILD = $(ROOT)/build

########################
# Application Targets
########################

CFG  = -DUSE_ARROW_PACKAGE=ON
CFG += -DUSE_AWS_PACKAGE=ON
CFG += -DUSE_CCSDS_PACKAGE=ON
CFG += -DUSE_GEO_PACKAGE=ON
CFG += -DUSE_H5_PACKAGE=ON
CFG += -DUSE_LEGACY_PACKAGE=ON
CFG += -DUSE_NETSVC_PACKAGE=ON
CFG += -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
CFG += -DENABLE_H5CORO_ATTRIBUTE_SUPPORT=ON

## reflect changes

all: ## build code
	make -j4 -C $(BUILD)

config: prep ## configure make for release version of sliderule
	cd $(BUILD); cmake -DCMAKE_BUILD_TYPE=Release $(CFG) $(ROOT)

config-debug: prep 
	cd $(BUILD); cmake -DCMAKE_BUILD_TYPE=Debug $(CFG) $(ROOT)
	$(eval CXXFLAGS += -g)
	
config-library: prep ## configure make for shared library libsliderule.so
	cd $(BUILD); cmake -DCMAKE_BUILD_TYPE=Release -DSHARED_LIBRARY=ON $(CFG) $(ROOT)

install: ## install sliderule to system
	make -C $(BUILD) install

uninstall: ## uninstall most recent install of sliderule from system
	xargs rm < $(BUILD)/install_manifest.txt

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

########################
# Global Targets
########################

ctags: ## generate ctags
	if [ -d ".clangd/index/" ]; then rm -f .clangd/index/*; fi             ## clear clagnd index (before clangd-11)
	if [ -d ".cache/clangd/index/" ]; then rm -f .cache/clangd/index/*; fi ## clear clagnd index (clangd-11)
	/usr/bin/jq -s 'add' $(SLIDERULE_BUILD)/compile_commands.json $(PGC_BUILD)/compile_commands.json $(ICESAT2_BUILD)/compile_commands.json $(GEDI_BUILD)/compile_commands.json $(LANDSAT_BUILD)/compile_commands.json $(USGS3DEP_BUILD)/compile_commands.json $(OPENDATA_BUILD)/compile_commands.json $(SWOT_BUILD)/compile_commands.json > compile_commands.json

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

