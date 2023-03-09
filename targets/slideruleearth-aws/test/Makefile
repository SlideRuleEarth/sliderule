ROOT = $(shell pwd)/../..
BUILD = $(ROOT)/build
STAGE = $(ROOT)/stage

SLIDERULE_SOURCE_DIR = $(ROOT)/../sliderule
SLIDERULE_BUILD_DIR = $(BUILD)/sliderule
ICESAT2_BUILD_DIR = $(BUILD)/icesat2
PGC_BUILD_DIR = $(BUILD)/pgc
GEDI_BUILD_DIR = $(BUILD)/gedi
SERVER_STAGE_DIR = $(STAGE)/sliderule

CLANG_VER = "-12"
CLANG_OPT = -DCMAKE_USER_MAKE_RULES_OVERRIDE=$(SLIDERULE_SOURCE_DIR)/platforms/linux/ClangOverrides.txt -D_CMAKE_TOOLCHAIN_PREFIX=llvm-
MYIP ?= $(shell (ip route get 1 | sed -n 's/^.*src \([0-9.]*\) .*$$/\1/p'))
ENVVER = $(shell git --git-dir ../../.git --work-tree ../../.git describe --abbrev --dirty --always --tags --long)

SLIDERULECFG := -DMAX_FREE_STACK_SIZE=1
SLIDERULECFG += -DUSE_ARROW_PACKAGE=ON
SLIDERULECFG += -DUSE_AWS_PACKAGE=ON
SLIDERULECFG += -DUSE_H5_PACKAGE=ON
SLIDERULECFG += -DUSE_NETSVC_PACKAGE=ON
SLIDERULECFG += -DUSE_GEO_PACKAGE=ON
SLIDERULECFG += -DUSE_LEGACY_PACKAGE=OFF
SLIDERULECFG += -DUSE_CCSDS_PACKAGE=OFF

TEST ?= $(SLIDERULE_SOURCE_DIR)/scripts/selftests/test_runner.lua

all: sliderule

sliderule: ## build the server using the local configuration
	make -j4 -C $(SLIDERULE_BUILD_DIR)
	make -C $(SLIDERULE_BUILD_DIR) install
	make -j4 -C $(ICESAT2_BUILD_DIR)
	make -C $(ICESAT2_BUILD_DIR) install
	make -j4 -C $(PGC_BUILD_DIR)
	make -C $(PGC_BUILD_DIR) install
	make -j4 -C $(GEDI_BUILD_DIR)
	make -C $(GEDI_BUILD_DIR) install
	cp $(ROOT)/docker/sliderule/asset_directory.csv $(SERVER_STAGE_DIR)/etc/sliderule
	cp $(ROOT)/docker/sliderule/empty.index $(SERVER_STAGE_DIR)/etc/sliderule

config-debug: prep ## configure the server for running locally with debug symbols
	cd $(SLIDERULE_BUILD_DIR); cmake -DCMAKE_BUILD_TYPE=Debug $(SLIDERULECFG) -DENABLE_TRACING=ON -DINSTALLDIR=$(SERVER_STAGE_DIR) $(SLIDERULE_SOURCE_DIR)
	cd $(ICESAT2_BUILD_DIR); cmake -DCMAKE_BUILD_TYPE=Debug -DINSTALLDIR=$(SERVER_STAGE_DIR) $(SLIDERULE_SOURCE_DIR)/plugins/icesat2
	cd $(PGC_BUILD_DIR); cmake -DCMAKE_BUILD_TYPE=Debug -DINSTALLDIR=$(SERVER_STAGE_DIR) $(SLIDERULE_SOURCE_DIR)/plugins/pgc
	cd $(GEDI_BUILD_DIR); cmake -DCMAKE_BUILD_TYPE=Debug -DINSTALLDIR=$(SERVER_STAGE_DIR) $(SLIDERULE_SOURCE_DIR)/plugins/gedi

config-release: prep ## configure server to run release version locally (useful for using valgrind)
	cd $(SLIDERULE_BUILD_DIR); cmake -DCMAKE_BUILD_TYPE=Release $(SLIDERULECFG) -DINSTALLDIR=$(SERVER_STAGE_DIR) $(SLIDERULE_SOURCE_DIR)
	cd $(ICESAT2_BUILD_DIR); cmake -DCMAKE_BUILD_TYPE=Release -DINSTALLDIR=$(SERVER_STAGE_DIR) $(SLIDERULE_SOURCE_DIR)/plugins/icesat2
	cd $(PGC_BUILD_DIR); cmake -DCMAKE_BUILD_TYPE=Release -DINSTALLDIR=$(SERVER_STAGE_DIR) $(SLIDERULE_SOURCE_DIR)/plugins/pgc
	cd $(GEDI_BUILD_DIR); cmake -DCMAKE_BUILD_TYPE=Release -DINSTALLDIR=$(SERVER_STAGE_DIR) $(SLIDERULE_SOURCE_DIR)/plugins/gedi

config-asan: prep ## configure server to run with address sanitizer locally
	cd $(SLIDERULE_BUILD_DIR); export CC=clang$(CLANG_VER); export CXX=clang++$(CLANG_VER); cmake -DCMAKE_BUILD_TYPE=Debug $(CLANG_OPT) -DENABLE_ADDRESS_SANITIZER=ON $(SLIDERULECFG) -DINSTALLDIR=$(SERVER_STAGE_DIR) $(SLIDERULE_SOURCE_DIR)
	cd $(ICESAT2_BUILD_DIR); export CC=clang$(CLANG_VER); export CXX=clang++$(CLANG_VER); cmake -DCMAKE_BUILD_TYPE=Debug $(CLANG_OPT) -DENABLE_ADDRESS_SANITIZER=ON -DINSTALLDIR=$(SERVER_STAGE_DIR) $(SLIDERULE_SOURCE_DIR)/plugins/icesat2
	cd $(PGC_BUILD_DIR); export CC=clang$(CLANG_VER); export CXX=clang++$(CLANG_VER); cmake -DCMAKE_BUILD_TYPE=Debug $(CLANG_OPT) -DENABLE_ADDRESS_SANITIZER=ON -DINSTALLDIR=$(SERVER_STAGE_DIR) $(SLIDERULE_SOURCE_DIR)/plugins/pgc
	cd $(GEDI_BUILD_DIR); export CC=clang$(CLANG_VER); export CXX=clang++$(CLANG_VER); cmake -DCMAKE_BUILD_TYPE=Debug $(CLANG_OPT) -DENABLE_ADDRESS_SANITIZER=ON -DINSTALLDIR=$(SERVER_STAGE_DIR) $(SLIDERULE_SOURCE_DIR)/plugins/gedi

prep: ## create temporary directories needed for build
	mkdir -p $(SLIDERULE_BUILD_DIR)
	mkdir -p $(ICESAT2_BUILD_DIR)
	mkdir -p $(PGC_BUILD_DIR)
	mkdir -p $(GEDI_BUILD_DIR)

run: ## run the server locally
	IPV4=$(MYIP) ENVIRONMENT_VERSION=$(ENVVER) $(SERVER_STAGE_DIR)/bin/sliderule $(SLIDERULE_SOURCE_DIR)/scripts/apps/server.lua $(ROOT)/test/cluster/config.json

selftest: ## run the self test on the server code
	$(SERVER_STAGE_DIR)/bin/sliderule $(TEST)

distclean: ## fully remove all non-version controlled files and directories
	- rm -Rf $(BUILD)
	- rm -Rf $(STAGE)

coreclean: ## clear out apport coredumps
	- rm /var/lib/apport/coredump/*

help: ## that's me!
	@printf "\033[37m%-30s\033[0m %s\n" "#-----------------------------------------------------------------------------------------"
	@printf "\033[37m%-30s\033[0m %s\n" "# Makefile Help                                                                          |"
	@printf "\033[37m%-30s\033[0m %s\n" "#-----------------------------------------------------------------------------------------"
	@printf "\033[37m%-30s\033[0m %s\n" "#-target-----------------------description------------------------------------------------"
	@grep -E '^[a-zA-Z_-].+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'


